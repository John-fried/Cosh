#include "../cosh.h"
#include "../wmcurses.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <vterm.h>
#include <utmp.h>


/* * TERMINAL EMULATOR CORE
 * Implements a grid-based terminal state machine with scrolling support.
 */

#define TERM_MAX_ROWS 1024

typedef struct {
        char	*grid[TERM_MAX_ROWS];
        int	rows, cols;
        int	cx, cy;             /* Virtual cursor position */
        int	scroll_top;         /* Current viewport top-most line */
} term_emu_t;

static term_emu_t *term_create(int r, int c)
{
        term_emu_t *t = calloc(1, sizeof(term_emu_t));
        t->rows = r;
        t->cols = c;
        for (int i = 0; i < r; i++)
                t->grid[i] = calloc(c + 1, sizeof(char));
        return t;
}

/* --- Iterm State --- */

typedef struct {
        VTerm *vt;
        VTermScreen *vts;
        term_emu_t *emu;
        int fd;
        pid_t pid;
        char cwd[128];
        int is_running;
} iterm_state_t;

/* --- Callbacks --- */

void app_iterm_input(cosh_win_t *win, int ch)
{
        iterm_state_t *st = (iterm_state_t *) win->priv;
        if (!st || !st->is_running || !st->emu)
                return;

	if (ch == WIN_MOUSE_SCROLL_UP) {
		if (st->emu->scroll_top > (st->emu->cy - st->emu->rows + win->vh) && st->emu->scroll_top > 0) 
			st->emu->scroll_top--;
		win->dirty = 1;
		return; 
	}

	if (ch == WIN_MOUSE_SCROLL_DOWN) {
		if (st->emu->scroll_top < st->emu->cy)
			st->emu->scroll_top++;
		win->dirty = 1;
		return;
	}

        VTermModifier mod = VTERM_MOD_NONE;

        if (ch >= KEY_MIN && ch <= KEY_MAX) {
                VTermKey key = VTERM_KEY_NONE;
                switch (ch) {
                case KEY_UP:        key = VTERM_KEY_UP;        break;
                case KEY_DOWN:      key = VTERM_KEY_DOWN;      break;
                case KEY_LEFT:      key = VTERM_KEY_LEFT;      break;
                case KEY_RIGHT:     key = VTERM_KEY_RIGHT;     break;
                case KEY_BACKSPACE: key = VTERM_KEY_BACKSPACE; break;
                case KEY_ENTER:
                case 10:            key = VTERM_KEY_ENTER;     break;
                }
                
                if (key != VTERM_KEY_NONE) {
                        vterm_keyboard_key(st->vt, key, mod);
                }
        } else if (ch < 0x110000) { 
                vterm_keyboard_unichar(st->vt, (uint32_t) ch, mod);
        }

	/* flush to pty */
        char buf[64];
        size_t len = vterm_output_read(st->vt, buf, sizeof(buf));
        if (len > 0)
                if (write(st->fd, buf, len) < 0) { 
			// handle error if necessary
                }
}

static int screen_pushline(int cols, const VTermScreenCell *cells, void *user)
{
        cosh_win_t *win = (cosh_win_t *)user;
        iterm_state_t *st = (iterm_state_t *)win->priv;
        term_emu_t *t = st->emu;

        char *old_line = t->grid[0];
        for (int i = 0; i < t->rows - 1; i++)
                t->grid[i] = t->grid[i + 1];
        
        t->grid[t->rows - 1] = old_line;

        for (int i = 0; i < cols && i < t->cols; i++) {
                t->grid[t->rows - 1][i] = cells[i].chars[0] ? (char)cells[i].chars[0] : ' ';
        }
        t->grid[t->rows - 1][cols < t->cols ? cols : t->cols] = '\0';

        if (t->cy < t->rows) t->cy++;

	st->emu->scroll_top = t->cy; 

        return 1;
}

static int screen_damage(VTermRect rect, void *user)
{
        (void)rect;
        ((cosh_win_t *) user)->dirty = 1;
        return 1;
}

static VTermScreenCallbacks screen_cbs = {
        .damage = screen_damage,
        .sb_pushline = screen_pushline,
};

void app_iterm_tick(cosh_win_t *win)
{
        iterm_state_t *st = (iterm_state_t *) win->priv;
        if (!st || !st->is_running)
                return;

        char buf[4096];
        // Baca semua data yang tersedia di PTY
        ssize_t n = read(st->fd, buf, sizeof(buf));
        if (n > 0) {
                // Tulis ke vterm. Ini akan otomatis memicu callback screen_damage
                // yang kemudian mengeset win->dirty = 1
                vterm_input_write(st->vt, buf, n);
		st->emu->scroll_top = st->emu->cy;
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                st->is_running = 0;
        }
}

void app_iterm_render(cosh_win_t *win)
{
        iterm_state_t *st = (iterm_state_t *) win->priv;
        if (!st || !st->emu) return;

        for (int row = 0; row < win->vh; row++) {
                int absolute_row = st->emu->scroll_top + row;

                if (absolute_row < st->emu->cy) {
                        int history_idx = (st->emu->rows - (st->emu->cy - absolute_row));
                        
                        if (history_idx >= 0 && history_idx < st->emu->rows) {
                                mvwprintw(win->ptr, row + 1, 2, "%.*s", win->vw, st->emu->grid[history_idx]);
                        }
                } else {
                        VTermPos pos;
                        pos.row = absolute_row - st->emu->cy;

                        if (pos.row >= 0 && pos.row < win->vh) {
                                for (pos.col = 0; pos.col < win->vw; pos.col++) {
                                        VTermScreenCell cell;
                                        if (vterm_screen_get_cell(st->vts, pos, &cell)) {
                                                uint32_t c = cell.chars[0] ? cell.chars[0] : ' ';
                                                mvwaddch(win->ptr, row + 1, pos.col + 2, c);
                                        }
                                }
                        }
                }
        }

        /* Tampilkan kursor hanya jika user melihat layar aktif (paling bawah) */
        if (st->emu->scroll_top >= st->emu->cy) {
                VTermPos curpos;
                vterm_state_get_cursorpos(vterm_obtain_state(st->vt), &curpos);
                
                int cursor_screen_row = (curpos.row + st->emu->cy) - st->emu->scroll_top;
                
                if (cursor_screen_row >= 0 && cursor_screen_row < win->vh) {
                        wattron(win->ptr, A_REVERSE);
                        mvwaddch(win->ptr, cursor_screen_row + 1, curpos.col + 2, ' ');
                        wattroff(win->ptr, A_REVERSE);
                }
        }
}

void iterm_cleanup(void *p)
{
        iterm_state_t *st = (iterm_state_t *) p;
        if (st->is_running)
                kill(st->pid, SIGKILL);
        vterm_free(st->vt);     // Bersihkan vterm
        free(st);
}

static void iterm_update_pty_size(iterm_state_t *st, int rows, int cols)
{
        struct winsize ws = {.ws_row = rows,.ws_col = cols };
        ioctl(st->fd, TIOCSWINSZ, &ws);
        vterm_set_size(st->vt, rows, cols);
}

void win_spawn_iterm(void)
{
        cosh_win_t *win = win_create(35, 120, WIN_FLAG_NONE);
        iterm_state_t *st = calloc(1, sizeof(iterm_state_t));

        // 1. Init VTerm dengan ukuran virtual window
	st->emu = term_create(TERM_MAX_ROWS, win->vw); 
        st->vt = vterm_new(win->vh, win->vw);
        st->vts = vterm_obtain_screen(st->vt);
        vterm_screen_set_callbacks(st->vts, &screen_cbs, win);
        vterm_screen_reset(st->vts, 1);

        // 2. Spawn Shell Langsung (Prompt akan muncul seketika)
        int master = posix_openpt(O_RDWR | O_NOCTTY);
        grantpt(master);
        unlockpt(master);
        st->fd = master;

        st->pid = fork();
        if (st->pid == 0) {
                int slave = open(ptsname(master), O_RDWR);
                login_tty(slave);       // Cara "Linus" yang bersih untuk setup PTY
                setenv("TERM", "xterm-256color", 1);    // Agar warna & scroll didukung
                execl("/bin/bash", "bash", NULL);       // -i untuk interactive prompt
                exit(1);
        }

        st->is_running = 1;
        fcntl(st->fd, F_SETFL, O_NONBLOCK);

        // Sinkronkan ukuran awal
        iterm_update_pty_size(st, win->vh, win->vw);

        win_setopt(win, WIN_OPT_TITLE, "Terminal");
        win_setopt(win, WIN_OPT_RENDER, app_iterm_render);
        win_setopt(win, WIN_OPT_INPUT, app_iterm_input);
        win_setopt(win, WIN_OPT_FG, COLOR_WHITE);
        win_setopt(win, WIN_OPT_BG, COLOR_BLACK);
        win_setopt(win, WIN_OPT_PRIV, st);
        win_setopt(win, WIN_OPT_TICK, app_iterm_tick);
}
