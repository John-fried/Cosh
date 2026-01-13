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

/* Terminal cell representation for history buffer. */
typedef struct {
        uint32_t chars[MAX_CELL_CHARS];
        int fg, bg;
} iterm_cell_t;

typedef struct {
        iterm_cell_t *cells;
        int cols;
} iterm_line_t;

typedef struct {
        VTerm *vt;
        VTermScreen *vts;
        int fd;
        pid_t pid;
        int active;
        int is_altscreen;

        iterm_line_t *history;
        int hist_head;
        int hist_cnt;

        int pairs[256][256];
} iterm_t;

static const struct {
	const char *seq;
	size_t len;
} altscreen_open_seq[] = {
    {"\x1b[?1049h", 8},
    {"\x1b[?1047h", 8},
    {"\x1b[?47h",   6},

    {"\033[?1049h", 8},
    {"\033[?1047h", 8},
    {"\033[?47h",   6}
};

/*  Internal Helpers  */

static inline int get_color_idx(VTermColor c)
{
        if (c.type != VTERM_COLOR_INDEXED)
                return -1;
        return ((unsigned char *)&c)[1];
}

static int get_pair(iterm_t *self, int vfg, int vbg)
{
        int fg = (vfg >= 0 && vfg < 256) ? vfg : COLOR_WHITE;
        int bg = (vbg >= 0 && vbg < 256) ? vbg : COLOR_BLACK;

        if (self->pairs[fg][bg] == 0) {
                int p = CP_WIN_START + 100 + (fg * 16 + bg);
                init_pair(p, fg, bg);
                self->pairs[fg][bg] = p;
        }
        return self->pairs[fg][bg];
}

/*  History Management  */

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
        cosh_win_t *win = (cosh_win_t *) user;
        iterm_t *self = (iterm_t *) win->priv;

        if (!self || !self->history || self->is_altscreen)
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

        win->scroll_max = self->hist_cnt;

        if (win->scroll_cur >= win->scroll_max - 1) {
                win->scroll_cur = win->scroll_max;
        }

        return 1;
}

static int cb_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
        (void)pos;
        (void)oldpos;
        (void)visible;
        if (user) {
                cosh_win_t *win = (cosh_win_t *) user;
                win->dirty = 1;
        }
        return 1;
}

static int cb_damage(VTermRect rect, void *user)
{
        (void)rect;
        if (user)
                ((cosh_win_t *) user)->dirty = 1;
        return 1;
}

static int cb_settermprop(VTermProp prop, VTermValue *val, void *user)
{
        cosh_win_t *win = (cosh_win_t *) user;
        iterm_t *self = (iterm_t *) win->priv;

	/* just handle closing altscreen only */
        if (prop == VTERM_PROP_ALTSCREEN && !val->boolean) {
		self->is_altscreen = 0;
                win->scroll_cur = self->hist_cnt;
                win->scroll_max = self->hist_cnt;

                werase(win->ptr);
                win->dirty = 1;
		win_needs_redraw = 1;
        }
        return 1;
}

static VTermScreenCallbacks screen_cbs = {
        .damage = cb_damage,
        .sb_pushline = cb_sb_pushline,
        .movecursor = cb_movecursor,
        .settermprop = cb_settermprop
};

/*  Lifecycle  */

static void iterm_sync_size(cosh_win_t *win)
{
        iterm_t *self = (iterm_t *) win->priv;
        if (!self || self->fd < 0)
                return;

        struct winsize ws = {.ws_row = (unsigned short)win->vh,.ws_col =
                    (unsigned short)win->vw
        };
        ioctl(self->fd, TIOCSWINSZ, &ws);
        vterm_set_size(self->vt, win->vh, win->vw);
        if (self->active)
                kill(self->pid, SIGWINCH);
}

static void iterm_spawn(iterm_t *self, int r, int c)
{
        struct winsize ws = {.ws_row = (unsigned short)r,.ws_col =
                    (unsigned short)c
        };
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

/*  App Callbacks  */

void app_iterm_tick(cosh_win_t *win)
{
        iterm_t *self = (iterm_t *) win->priv;
        char buf[READ_BUF_SIZE];

        if (!self || !self->active || self->fd < 0)
                return;

        ssize_t n = read(self->fd, buf, sizeof(buf));
        if (n > 0) {
		/* This is fucking shit best */
		size_t altscreen_open_count = sizeof(altscreen_open_seq) / sizeof(altscreen_open_seq[0]);
		for (size_t i = 0; i < altscreen_open_count; i++) {
			if (memmem(buf, n, altscreen_open_seq[i].seq, altscreen_open_seq[i].len)) {
				if (!self->is_altscreen) {
					self->is_altscreen = 1;
					win->scroll_cur = 0;
					win->scroll_max = 0;
					werase(win->ptr);
					win->dirty = 1;
					win_needs_redraw = 1;
					break;
				}
			}
		}

                vterm_input_write(self->vt, buf, (size_t)n);
	} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                self->active = 0;
}

void app_iterm_input(cosh_win_t *win, int ch)
{
        iterm_t *self = (iterm_t *) win->priv;
        if (!self || !self->active)
                return;

        if (self->is_altscreen) {
                if (ch == WIN_MOUSE_SCROLL_UP) {
                        vterm_keyboard_key(self->vt, VTERM_KEY_UP,
                                           VTERM_MOD_NONE);
                        goto send_output;
                } else if (ch == WIN_MOUSE_SCROLL_DOWN) {
                        vterm_keyboard_key(self->vt, VTERM_KEY_DOWN,
                                           VTERM_MOD_NONE);
                        goto send_output;
                }
        } else {
                if (ch == WIN_MOUSE_SCROLL_UP) {
                        if (win->scroll_cur > 0)
                                win->scroll_cur =
                                    (win->scroll_cur >
                                     2) ? win->scroll_cur - 2 : 0;
                        win->dirty = 1;
                        return;
                } else if (ch == WIN_MOUSE_SCROLL_DOWN) {
                        if (win->scroll_cur < win->scroll_max)
                                win->scroll_cur =
                                    (win->scroll_cur + 2 >
                                     win->scroll_max) ? win->scroll_max : win->
                                    scroll_cur + 2;
                        win->dirty = 1;
                        return;
                }
        }

        win->scroll_cur = win->scroll_max;

	if (ch >= KEY_MIN && ch <= KEY_MAX) {
		VTermKey k = VTERM_KEY_NONE;
		switch (ch) {
		    case KEY_UP:        k = VTERM_KEY_UP; break;
		    case KEY_DOWN:      k = VTERM_KEY_DOWN; break;
		    case KEY_LEFT:      k = VTERM_KEY_LEFT; break;
		    case KEY_RIGHT:     k = VTERM_KEY_RIGHT; break;
		    case KEY_BACKSPACE: k = VTERM_KEY_BACKSPACE; break;
		    case KEY_DC:        k = VTERM_KEY_DEL; break;
		    case KEY_IC:        k = VTERM_KEY_INS; break;
		    case KEY_HOME:      k = VTERM_KEY_HOME; break;
		    case KEY_END:       k = VTERM_KEY_END; break;
		    case KEY_PPAGE:     k = VTERM_KEY_PAGEUP; break;
		    case KEY_NPAGE:     k = VTERM_KEY_PAGEDOWN; break;
		    case KEY_ENTER:
		    case 10:            k = VTERM_KEY_ENTER; break;
		}
		if (k != VTERM_KEY_NONE)
		    vterm_keyboard_key(self->vt, k, VTERM_MOD_NONE);
	} else {
		vterm_keyboard_unichar(self->vt, (uint32_t) ch, VTERM_MOD_NONE);
	}

send_output:{
		char out[128];
		size_t len = vterm_output_read(self->vt, out, sizeof(out));
                if (len > 0)
                        (void)write(self->fd, out, len);
        }
}

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

	VTermPos cur;
        vterm_state_get_cursorpos(vterm_obtain_state(self->vt), &cur);

	if (self->is_altscreen) {
		win->scroll_max = 0;
		win->scroll_cur = 0;

		for (int r = 0; r < rows; r++) {
			for (int c = 0; c < cols; c++) {
				VTermPos pos = {.row = r, .col = c};
				VTermScreenCell cell;

				if (vterm_screen_get_cell(self->vts, pos, &cell)) {
					int p = (r == cur.row && c == cur.col) ?
						CP_CURSOR : get_pair(self, get_color_idx(cell.fg), get_color_idx(cell.bg));

					win_attron(win, p);
					draw_cell(win->ptr, r + 1, c + 1, cell.chars, p);
					win_attroff(win, p);
				}
			}
		}

		return; /* early return */
	}

	win->scroll_max = self->hist_cnt;
        int scroll_offset = self->is_altscreen ? 0 : (self->hist_cnt - win->scroll_cur);
	if (scroll_offset < 0)
                scroll_offset = 0;

        for (int r = 0; r < rows; r++) {
		if (!self->is_altscreen && r < scroll_offset) {
			/* Render from History */
			int hidx =
			    (self->hist_head - (scroll_offset - r) + 1 +
			     HIST_SIZE) % HIST_SIZE;
			iterm_line_t *l = &self->history[hidx];

			if (l && l->cells) {
				for (int c = 0; c < cols && c < l->cols; c++) {
					int p = get_pair(self, l->cells[c].fg,
							 l->cells[c].bg);
					win_attron(win, p);
					draw_cell(win->ptr, r + 1, c + 2,
						  l->cells[c].chars, p);
					win_attroff(win, p);
				}
			}
			continue;
		}

		/* Render Active Screen */
		VTermPos pos = {.row =
			    self->is_altscreen ? r : (r - scroll_offset),.col =
			    0 };
		for (pos.col = 0; pos.col < cols; pos.col++) {
			VTermScreenCell cell;
			if (vterm_screen_get_cell(self->vts, pos, &cell)) {
				int p;
				if (scroll_offset == 0 && pos.row == cur.row
				    && pos.col == cur.col) {
					p = CP_CURSOR;
				} else {
					p = get_pair(self,
						     get_color_idx(cell.fg),
						     get_color_idx(cell.bg));
				}

				win_attron(win, p);
				draw_cell(win->ptr, r + 1, pos.col + 2,
					  cell.chars, p);
				win_attroff(win, p);
			}
		}
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
        win_setopt(win, WIN_OPT_CURSOR, 0);
}
