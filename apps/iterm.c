#include "../cosh.h"
#include "../wmcurses.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <vterm.h>
#include <utmp.h>
#include <pty.h>

#define HIST_SIZE	4096
#define READ_BUF_SIZE	16384
#define MAX_CELL_CHARS	6

/*
 * Terminal cell representation for history buffer.
 */
typedef struct {
        uint32_t chars[MAX_CELL_CHARS];
        int fg, bg;
} iterm_cell_t;

typedef struct {
        iterm_cell_t *cells;
        int cols;
} iterm_line_t;

/*
 * Iterm Core Object (OOP Design)
 */
typedef struct {
        VTerm *vt;
        VTermScreen *vts;
        int fd;
        pid_t pid;
        int active;

        /* Circular history buffer */
        iterm_line_t *history;
        int hist_head;
        int hist_cnt;
        int scroll_off;

        /* Pair cache: 0-255 mapping for ncurses */
        int pairs[16][16];
} iterm_t;

/* --- Internal Helpers --- */

/**
 * Access color index directly from memory to bypass inconsistent VTermColor 
 * member naming across different libvterm versions (Termux/Debian/Arch).
 * Verified for libvterm 0.3.3.
 */
static inline int get_color_idx(VTermColor c)
{
        if (c.type != VTERM_COLOR_INDEXED)
                return -1;
        /* Memory layout: [type:1byte][index:1byte] ... */
        return ((unsigned char *)&c)[1];
}

static int get_pair(iterm_t *self, int vfg, int vbg)
{
        int fg = (vfg >= 0 && vfg < 16) ? vfg : COLOR_WHITE;
        int bg = (vbg >= 0 && vbg < 16) ? vbg : COLOR_BLACK;

        if (self->pairs[fg][bg] == 0) {
                int p = CP_WIN_START + 100 + (fg * 16 + bg);
                init_pair(p, fg, bg);
                self->pairs[fg][bg] = p;
        }
        return self->pairs[fg][bg];
}

/* --- History Management --- */

static void free_line(iterm_line_t *line)
{
        if (line && line->cells)
                free(line->cells);
        if (line) {
                line->cells = NULL;
                line->cols = 0;
        }
}

static int cb_sb_pushline(int cols, const VTermScreenCell *cells, void *user)
{
        iterm_t *self = (iterm_t *) user;
        if (!self || !self->history)
                return 1;

        int idx = (self->hist_head + 1) % HIST_SIZE;

        free_line(&self->history[idx]);
        self->history[idx].cells = malloc(sizeof(iterm_cell_t) * cols);
        if (!self->history[idx].cells)
                return 0;

        self->history[idx].cols = cols;

        for (int i = 0; i < cols; i++) {
                memcpy(self->history[idx].cells[i].chars, cells[i].chars,
                       sizeof(uint32_t) * MAX_CELL_CHARS);
                self->history[idx].cells[i].fg = get_color_idx(cells[i].fg);
                self->history[idx].cells[i].bg = get_color_idx(cells[i].bg);
        }

        self->hist_head = idx;
        if (self->hist_cnt < HIST_SIZE)
                self->hist_cnt++;

        return 1;
}

static int cb_damage(VTermRect rect, void *user)
{
        (void)rect;
        if (user)
                ((cosh_win_t *) user)->dirty = 1;
        return 1;
}

static VTermScreenCallbacks screen_cbs = {
        .damage = cb_damage,
        .sb_pushline = cb_sb_pushline,
};

/* --- Lifecycle --- */

static void iterm_sync_size(cosh_win_t *win)
{
        iterm_t *self = (iterm_t *) win->priv;
        if (!self || self->fd < 0)
                return;

        struct winsize ws = {.ws_row = (unsigned short)win->vh,.ws_col =
                    (unsigned short)win->vw };
        ioctl(self->fd, TIOCSWINSZ, &ws);
        vterm_set_size(self->vt, win->vh, win->vw);
        if (self->active)
                kill(self->pid, SIGWINCH);
}

static void iterm_spawn(iterm_t *self, int r, int c)
{
        struct winsize ws = {.ws_row = (unsigned short)r,.ws_col =
                    (unsigned short)c };
        self->pid = forkpty(&self->fd, NULL, NULL, &ws);

        if (self->pid == 0) {
                setenv("TERM", "xterm-256color", 1);
                setenv("LANG", "en_US.UTF-8", 1);
                execl("/bin/bash", "bash", NULL);
                _exit(1);
        }

        self->active = 1;
        fcntl(self->fd, F_SETFL, O_NONBLOCK);
}

/* --- App Callbacks --- */

void app_iterm_tick(cosh_win_t *win)
{
        iterm_t *self = (iterm_t *) win->priv;
        char buf[READ_BUF_SIZE];

        if (!self || !self->active || self->fd < 0)
                return;

        ssize_t n = read(self->fd, buf, sizeof(buf));
        if (n > 0)
                vterm_input_write(self->vt, buf, (size_t)n);
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                self->active = 0;
}

void app_iterm_input(cosh_win_t *win, int ch)
{
        iterm_t *self = (iterm_t *) win->priv;
        if (!self || !self->active)
                return;

        if (ch == WIN_MOUSE_SCROLL_UP) {
                if (self->scroll_off < self->hist_cnt)
                        self->scroll_off += 2;
                win->dirty = 1;
                return;
        } else if (ch == WIN_MOUSE_SCROLL_DOWN) {
                self->scroll_off =
                    (self->scroll_off > 2) ? self->scroll_off - 2 : 0;
                win->dirty = 1;
                return;
        }

        self->scroll_off = 0;

        if (ch >= KEY_MIN && ch <= KEY_MAX) {
                VTermKey k = VTERM_KEY_NONE;
                switch (ch) {
                case KEY_UP:
                        k = VTERM_KEY_UP;
                        break;
                case KEY_DOWN:
                        k = VTERM_KEY_DOWN;
                        break;
                case KEY_LEFT:
                        k = VTERM_KEY_LEFT;
                        break;
                case KEY_RIGHT:
                        k = VTERM_KEY_RIGHT;
                        break;
                case KEY_BACKSPACE:
                        k = VTERM_KEY_BACKSPACE;
                        break;
                case KEY_ENTER:
                case 10:
                        k = VTERM_KEY_ENTER;
                        break;
                }
                if (k != VTERM_KEY_NONE)
                        vterm_keyboard_key(self->vt, k, VTERM_MOD_NONE);
        } else {
                vterm_keyboard_unichar(self->vt, (uint32_t) ch, VTERM_MOD_NONE);
        }

        char out[128];
        size_t len = vterm_output_read(self->vt, out, sizeof(out));
        if (len > 0)
                (void)write(self->fd, out, len);
}

/**
 * Robust UTF-8 cell drawing.
 * Fixes caret artifacts (^) by ensuring empty cells render as spaces.
 */
static void draw_cell(WINDOW *w, int y, int x, uint32_t *chars, int pair)
{
        cchar_t wc;
        wchar_t wstr[MAX_CELL_CHARS + 1];
        int i;

        /* If codepoint is 0 or invalid, render a space to avoid ^@ artifacts */
        if (chars[0] == 0) {
                wstr[0] = L' ';
                wstr[1] = L'\0';
        } else {
                for (i = 0; i < MAX_CELL_CHARS && chars[i]; i++)
                        wstr[i] = (wchar_t)chars[i];
                wstr[i] = L'\0';
        }

        if (setcchar(&wc, wstr, A_NORMAL, (short)pair, NULL) == OK)
                mvwadd_wch(w, y, x, &wc);
}

void app_iterm_render(cosh_win_t *win)
{
        iterm_t *self = (iterm_t *) win->priv;
        if (!self || !win->ptr)
                return;

        int rows = win->vh;
        int cols = win->vw;

        for (int r = 0; r < rows; r++) {
                if (r < self->scroll_off) {
                        /* History rendering from circular buffer */
                        int hidx =
                            (self->hist_head - (self->scroll_off - r) + 1 +
                             HIST_SIZE) % HIST_SIZE;
                        iterm_line_t *l = &self->history[hidx];
                        if (l && l->cells) {
                                for (int c = 0; c < cols && c < l->cols; c++) {
                                        int p =
                                            get_pair(self, l->cells[c].fg,
                                                     l->cells[c].bg);
                                        draw_cell(win->ptr, r + 1, c + 2,
                                                  l->cells[c].chars, p);
                                }
                        }
                        continue;
                }

                /* Active terminal screen rendering */
                VTermPos pos = {.row = r - self->scroll_off,.col = 0 };
                for (pos.col = 0; pos.col < cols; pos.col++) {
                        VTermScreenCell cell;
                        if (vterm_screen_get_cell(self->vts, pos, &cell)) {
                                int p =
                                    get_pair(self, get_color_idx(cell.fg),
                                             get_color_idx(cell.bg));
                                draw_cell(win->ptr, r + 1, pos.col + 2,
                                          cell.chars, p);
                        }
                }
        }

        /* Display cursor only when not scrolling back */
        if (self->scroll_off == 0) {
                VTermPos cur;
                vterm_state_get_cursorpos(vterm_obtain_state(self->vt), &cur);
                if (cur.row >= 0 && cur.row < rows && cur.col >= 0
                    && cur.col < cols)
                        mvwchgat(win->ptr, cur.row + 1, cur.col + 2, 1,
                                 A_REVERSE, 0, NULL);
        }
}

void iterm_cleanup(void *p)
{
        iterm_t *self = (iterm_t *) p;
        if (!self)
                return;

        if (self->active && self->pid > 0) {
                kill(self->pid, SIGHUP);
                usleep(1000);
                kill(self->pid, SIGKILL);
                waitpid(self->pid, NULL, WNOHANG);
        }

        if (self->history) {
                for (int i = 0; i < HIST_SIZE; i++)
                        free_line(&self->history[i]);
                free(self->history);
        }

        if (self->vt)
                vterm_free(self->vt);

        if (self->fd >= 0)
                close(self->fd);

        free(self);
}

void win_spawn_iterm(void)
{
        cosh_win_t *win = win_create(35, 120, WIN_FLAG_NONE);
        if (!win)
                return;

        iterm_t *self = calloc(1, sizeof(iterm_t));
        if (!self)
                return;

        self->vt = vterm_new(win->vh, win->vw);
        vterm_set_utf8(self->vt, 1);
        self->vts = vterm_obtain_screen(self->vt);
        self->history = calloc(HIST_SIZE, sizeof(iterm_line_t));
        self->fd = -1;

        win_setopt(win, WIN_OPT_PRIV, self);
        vterm_screen_set_callbacks(self->vts, &screen_cbs, win);
        vterm_screen_reset(self->vts, 1);

        iterm_spawn(self, win->vh, win->vw);

        win_setopt(win, WIN_OPT_TITLE, "Terminal");
        win_setopt(win, WIN_OPT_RENDER, app_iterm_render);
        win_setopt(win, WIN_OPT_INPUT, app_iterm_input);
        win_setopt(win, WIN_OPT_FG, COLOR_WHITE);
        win_setopt(win, WIN_OPT_BG, COLOR_BLACK);
        win_setopt(win, WIN_OPT_TICK, app_iterm_tick);
        win_setopt(win, WIN_OPT_DESTROY, iterm_cleanup);
        win_setopt(win, WIN_OPT_RESIZE, iterm_sync_size);
}
