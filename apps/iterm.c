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
#include <vterm.h>
#include <utmp.h>

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
        char *grid[TERM_MAX_ROWS];
        int rows, cols;
        int cx, cy;             /* Virtual cursor position */
        int scroll_top;         /* Current viewport top-most line */
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

static void term_scroll_internal(term_emu_t *t)
{
        char *tmp = t->grid[0];
        for (int i = 0; i < t->rows - 1; i++)
                t->grid[i] = t->grid[i + 1];
        t->grid[t->rows - 1] = tmp;
        memset(t->grid[t->rows - 1], ' ', t->cols);
        if (t->cy > 0)
                t->cy--;
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
                if (t->cy >= t->rows)
                        term_scroll_internal(t);
                /* Auto-follow cursor if it moves out of current view */
                if (t->cy >= t->scroll_top + vh)
                        t->scroll_top = t->cy - vh + 1;
                return;
        }
        if (c == '\b' || c == 127) {
                if (t->cx > 0)
                        t->cx--;
                return;
        }
        if (c == '\t') {
                t->cx = (t->cx + 8) & ~7;
                return;
        }

        if (t->cx >= t->cols) {
                t->cx = 0;
                t->cy++;
                if (t->cy >= t->rows)
                        term_scroll_internal(t);
                if (t->cy >= t->scroll_top + vh)
                        t->scroll_top = t->cy - vh + 1;
        }

        t->grid[t->cy][t->cx++] = c;
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

static void iterm_jit_exec(iterm_state_t *st, const char *code, int vh)
{
        FILE *fp = fopen(JIT_SRC, "w");
        if (!fp)
                return;
        fprintf(fp,
                "#include <stdio.h>\n#include <stdlib.h>\nvoid _run() { %s }\n",
                code);
        fclose(fp);

        if (system
            ("gcc -O2 -fPIC -shared -o " JIT_BIN " " JIT_SRC " > " JIT_LOG
             " 2>&1") == 0) {
                void *h = dlopen(JIT_BIN, RTLD_NOW);
                if (h) {
                        void (*fn)() = dlsym(h, "_run");
                        if (fn)
                                fn();
                        dlclose(h);
                }
        } else {
                char buf[256];
                FILE *l = fopen(JIT_LOG, "r");
                while (l && fgets(buf, sizeof(buf), l)) {
                        for (int i = 0; buf[i]; i++)
                                term_write_char(st->emu, buf[i], vh);
                }
                if (l)
                        fclose(l);
        }
        unlink(JIT_SRC);
        unlink(JIT_BIN);
}

static void iterm_spawn_cmd(iterm_state_t *st, char *cmd)
{
        int master;
        struct termios tio;

        if (strncmp(cmd, "cd ", 3) == 0) {
                if (chdir(cmd + 3) == 0)
                        getcwd(st->cwd, sizeof(st->cwd));
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

                dup2(slave, 0);
                dup2(slave, 1);
                dup2(slave, 2);
                close(slave);
                close(master);

                setsid();
                ioctl(0, TIOCSCTTY, 1);

                setenv("TERM", "vt100", 1);
                char *args[] = { "/bin/sh", "-c", cmd, NULL };
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
        iterm_state_t *st = (iterm_state_t *) win->priv;
        if (!st || !st->is_running)
                return;

        // Kirim input langsung ke libvterm untuk diterjemahkan jadi Escape Sequence
        VTermModifier mod = VTERM_MOD_NONE;

        // Mapping kunci khusus
        if (ch >= KEY_MIN && ch <= KEY_MAX) {
                VTermKey key = VTERM_KEY_NONE;
                switch (ch) {
                case KEY_UP:
                        key = VTERM_KEY_UP;
                        break;
                case KEY_DOWN:
                        key = VTERM_KEY_DOWN;
                        break;
                case KEY_LEFT:
                        key = VTERM_KEY_LEFT;
                        break;
                case KEY_RIGHT:
                        key = VTERM_KEY_RIGHT;
                        break;
                case KEY_BACKSPACE:
                        key = VTERM_KEY_BACKSPACE;
                        break;
                case KEY_ENTER:
                case 10:
                        key = VTERM_KEY_ENTER;
                        break;
                }
                if (key != VTERM_KEY_NONE) {
                        vterm_keyboard_key(st->vt, key, mod);
                }
        } else {
                // Karakter biasa (ASCII)
                vterm_keyboard_unichar(st->vt, (uint32_t) ch, mod);
        }

        // Ambil string hasil terjemahan vterm, kirim ke PTY
        char buf[64];
        size_t len = vterm_output_read(st->vt, buf, sizeof(buf));
        if (len > 0)
                write(st->fd, buf, len);
}

static int screen_damage(VTermRect rect, void *user)
{
        (void)rect;
        // Memberitahu wmcurses bahwa window ini perlu di-render ulang
        ((cosh_win_t *) user)->dirty = 1;
        return 1;
}

static VTermScreenCallbacks screen_cbs = {
        .damage = screen_damage,
        // Kita bisa menambahkan .movecursor atau .settermprop jika perlu
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
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                st->is_running = 0;
        }
}

void app_iterm_render(cosh_win_t *win)
{
        iterm_state_t *st = (iterm_state_t *) win->priv;
        VTermPos pos;

        for (pos.row = 0; pos.row < win->h - 2; pos.row++) {
                for (pos.col = 0; pos.col < win->w - 4; pos.col++) {
                        VTermScreenCell cell;
                        if (vterm_screen_get_cell(st->vts, pos, &cell)) {
                                // Ambil karakter pertama dari cell (mendukung UTF-8 dasar)
                                uint32_t c = cell.chars[0];
                                if (c == 0)
                                        c = ' ';

                                mvwaddch(win->ptr, pos.row + 1, pos.col + 2, c);
                        }
                }
        }

        // Tampilkan kursor dari state libvterm
        VTermPos curpos;
        vterm_state_get_cursorpos(vterm_obtain_state(st->vt), &curpos);
        wattron(win->ptr, A_REVERSE);
        mvwaddch(win->ptr, curpos.row + 1, curpos.col + 2, ' ');
        wattroff(win->ptr, A_REVERSE);
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
