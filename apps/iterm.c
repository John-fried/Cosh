#include "../cosh.h"
#include "../wmcurses.h"

#define _XOPEN_SOURCE 600
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

/* JIT constants */
#define JIT_SRC		WORKDIR"/jit_temp.c"
#define JIT_BIN		WORKDIR"/jit_temp.so"
#define JIT_LOG		WORKDIR"/jit_temp.log"

/* * TERMINAL EMULATOR CORE
 * Implements a grid-based terminal state machine with scrolling support.
 */

#define TERM_MAX_ROWS 1024
#define TERM_MAX_COLS 256

typedef struct {
	char	*grid[TERM_MAX_ROWS];
	int	rows, cols;
	int	cx, cy;		/* Virtual cursor position */
	int	scroll_top;	/* Current viewport top-most line */
} term_emu_t;

static term_emu_t *term_create(int r, int c)
{
	term_emu_t *t = calloc(1, sizeof(term_emu_t));
	t->rows = r; t->cols = c;
	for (int i = 0; i < r; i++)
		t->grid[i] = calloc(c + 1, sizeof(char));
	return t;
}

static void term_scroll_internal(term_emu_t *t)
{
	char *tmp = t->grid[0];
	for (int i = 0; i < t->rows - 1; i++)
		t->grid[i] = t->grid[i+1];
	t->grid[t->rows - 1] = tmp;
	memset(t->grid[t->rows - 1], ' ', t->cols);
	if (t->cy > 0) t->cy--;
}

static void term_write_char(term_emu_t *t, char c, int vh)
{
	if (c == '\r') {
		t->cx = 0;
		return;
	}
	if (c == '\n') {
		t->cx = 0;
		t->cy++;
		if (t->cy >= t->rows) term_scroll_internal(t);
		/* Auto-follow cursor if it moves out of current view */
		if (t->cy >= t->scroll_top + vh) t->scroll_top = t->cy - vh + 1;
		return;
	}
	if (c == '\b' || c == 127) {
		if (t->cx > 0) t->cx--;
		return;
	}
	if (c == '\t') {
		t->cx = (t->cx + 8) & ~7;
		return;
	}

	if (t->cx >= t->cols) {
		t->cx = 0;
		t->cy++;
		if (t->cy >= t->rows) term_scroll_internal(t);
		if (t->cy >= t->scroll_top + vh) t->scroll_top = t->cy - vh + 1;
	}

	t->grid[t->cy][t->cx++] = c;
}

/* --- Iterm State --- */

typedef struct {
	term_emu_t	*emu;
	int		fd;
	pid_t		pid;
	char		cwd[128];
	int		is_running;
} iterm_state_t;

static void iterm_jit_exec(iterm_state_t *st, const char *code, int vh)
{
	FILE *fp = fopen(JIT_SRC, "w");
	if (!fp) return;
	fprintf(fp, "#include <stdio.h>\n#include <stdlib.h>\nvoid _run() { %s }\n", code);
	fclose(fp);

	if (system("gcc -O2 -fPIC -shared -o "JIT_BIN" "JIT_SRC" > "JIT_LOG" 2>&1") == 0) {
		void *h = dlopen(JIT_BIN, RTLD_NOW);
		if (h) {
			void (*fn)() = dlsym(h, "_run");
			if (fn) fn();
			dlclose(h);
		}
	} else {
		char buf[256];
		FILE *l = fopen(JIT_LOG, "r");
		while (l && fgets(buf, sizeof(buf), l)) {
			for (int i = 0; buf[i]; i++) term_write_char(st->emu, buf[i], vh);
		}
		if (l) fclose(l);
	}
	unlink(JIT_SRC); unlink(JIT_BIN);
}

static void iterm_spawn_cmd(iterm_state_t *st, char *cmd)
{
	int master;
	struct termios tio;

	if (strncmp(cmd, "cd ", 3) == 0) {
		if (chdir(cmd + 3) == 0) getcwd(st->cwd, sizeof(st->cwd));
		return;
	}

	master = posix_openpt(O_RDWR | O_NOCTTY);
	grantpt(master);
	unlockpt(master);

	st->pid = fork();
	if (st->pid == 0) {
		int slave = open(ptsname(master), O_RDWR);
		tcgetattr(slave, &tio);
		cfmakeraw(&tio);
		tcsetattr(slave, TCSANOW, &tio);

		dup2(slave, 0); dup2(slave, 1); dup2(slave, 2);
		close(slave); close(master);

		setsid();
		ioctl(0, TIOCSCTTY, 1);

		setenv("TERM", "vt100", 1);
		char *args[] = {"/bin/sh", "-c", cmd, NULL};
		execv(args[0], args);
		exit(1);
	}

	st->fd = master;
	st->is_running = 1;
	fcntl(st->fd, F_SETFL, O_NONBLOCK);
}

/* --- Callbacks --- */

void app_iterm_input(cosh_win_t *win, int ch)
{
	iterm_state_t *st = (iterm_state_t *)win->priv;
	int vh = win->h - 2;

	/* Forward input to active process */
	if (st->is_running) {
		char c = (char)ch;
		if (ch == KEY_ENTER) c = '\n';
		
		/* Intercept mouse wheel to scroll history even during process execution */
		if (ch == KEY_UP) {
			if (st->emu->scroll_top > 0) st->emu->scroll_top--;
			return;
		}
		if (ch == KEY_DOWN) {
			if (st->emu->scroll_top < st->emu->cy) st->emu->scroll_top++;
			return;
		}

		write(st->fd, &c, 1);
		return;
	}

	static char cmd[256];
	static int cp = 0;

	switch (ch) {
	case '\n':
	case KEY_ENTER:
		cmd[cp] = '\0';
		term_write_char(st->emu, '\n', vh);
		
		if (cmd[strlen(cmd)-1] == ';') iterm_jit_exec(st, cmd, vh);
		else iterm_spawn_cmd(st, cmd);
		
		cp = 0; cmd[0] = '\0';
		break;

	case KEY_BACKSPACE:
	case 127:
		if (cp > 0) {
			cp--;
			term_write_char(st->emu, '\b', vh);
		}
		break;

	case KEY_UP:
		if (st->emu->scroll_top > 0) st->emu->scroll_top--;
		break;

	case KEY_DOWN:
		if (st->emu->scroll_top < st->emu->cy) st->emu->scroll_top++;
		break;

	default:
		if (ch >= 32 && ch <= 126 && cp < 255) {
			cmd[cp++] = (char)ch;
			term_write_char(st->emu, (char)ch, vh);
		}
		break;
	}
}

void app_iterm_render(cosh_win_t *win)
{
	iterm_state_t *st = (iterm_state_t *)win->priv;
	int vh = win->h - 2;
	char buf[4096];
	ssize_t n;

	if (st->is_running) {
		struct winsize ws = { .ws_row = (unsigned short)vh, .ws_col = (unsigned short)(win->w - 4) };
		ioctl(st->fd, TIOCSWINSZ, &ws);

		while ((n = read(st->fd, buf, sizeof(buf))) > 0) {
			for (int i = 0; i < n; i++) {
				/* Simple ANSI Stripper */
				if (buf[i] == '\033') {
					while (i < n && !((buf[i] >= 'A' && buf[i] <= 'Z') || (buf[i] >= 'a' && buf[i] <= 'z'))) i++;
					continue;
				}
				term_write_char(st->emu, buf[i], vh);
			}
		}

		int status;
		if (waitpid(st->pid, &status, WNOHANG) != 0) {
			st->is_running = 0;
			close(st->fd);
			char p[256]; 
			snprintf(p, sizeof(p), "\n[%s] # ", st->cwd);
			for(int i=0; p[i]; i++) term_write_char(st->emu, p[i], vh);
		}
	}

	/* Render Grid Viewport based on scroll_top */
	for (int i = 0; i < vh; i++) {
		int line_idx = st->emu->scroll_top + i;
		if (line_idx >= st->emu->rows) break;
		mvwprintw(win->ptr, i + 1, 2, "%.*s", win->w - 4, st->emu->grid[line_idx]);
	}

	/* Render Cursor if within viewport */
	if (!st->is_running) {
		int cur_v_y = st->emu->cy - st->emu->scroll_top;
		if (cur_v_y >= 0 && cur_v_y < vh) {
			wattron(win->ptr, A_REVERSE);
			mvwaddch(win->ptr, cur_v_y + 1, 2 + st->emu->cx, 
				 st->emu->grid[st->emu->cy][st->emu->cx]);
			wattroff(win->ptr, A_REVERSE);
		}
	}
}

void iterm_cleanup(void *p)
{
	iterm_state_t *st = (iterm_state_t *)p;
	if (st->is_running) kill(st->pid, SIGKILL);
	for (int i = 0; i < st->emu->rows; i++) free(st->emu->grid[i]);
	free(st->emu); free(st);
}

void win_spawn_iterm(void)
{
	iterm_state_t *st = calloc(1, sizeof(iterm_state_t));
	st->emu = term_create(TERM_MAX_ROWS, TERM_MAX_COLS);
	getcwd(st->cwd, sizeof(st->cwd));

	int vh = 33; /* Default spawn height estimation */
	char init_msg[256];
	snprintf(init_msg, sizeof(init_msg), "ADAMS SHELL v4.1\n[%s] # ", st->cwd);
	for(int i=0; init_msg[i]; i++) term_write_char(st->emu, init_msg[i], vh);

	cosh_win_t *win = win_create(35, 120, WIN_FLAG_NONE);
	win_setopt(win, WIN_OPT_TITLE,   "Adams Terminal");
	win_setopt(win, WIN_OPT_RENDER,  app_iterm_render);
	win_setopt(win, WIN_OPT_INPUT,   app_iterm_input);
	win_setopt(win, WIN_OPT_PRIV,    st);
	win_setopt(win, WIN_OPT_DESTROY, iterm_cleanup);
}
