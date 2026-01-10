#include "cosh.h"

void win_spawn_iterm(void);
void win_spawn_palette(void);
void win_spawn_help(void);

app_entry_t *app_registry = NULL;
int app_count = 0;
struct workdir_state *wstate = NULL;

void register_app(const char *name, void (*spawn)(void))
{
	k_log_trace("  registered app: %s", name);
        app_entry_t *new_app = malloc(sizeof(app_entry_t));
        new_app->name = strdup(name);
        new_app->spawn = spawn;
        new_app->score = 0;
        new_app->next = app_registry;
        app_registry = new_app;
        app_count++;

}

void app_shutdown_render(cosh_win_t *win)
{
        mvwprintw(win->ptr, 1, (win->w - 16) / 2, "CONFIRM SHUTDOWN");
        mvwprintw(win->ptr, 3, (win->w - 22) / 2, "Press [y] Yes | [n] No");
}

int confirm_shutdown(void)
{
        int h = 5, w = 40, res = 0;
        cosh_win_t *dlg = win_create(h, w, WIN_FLAG_LOCKED);
        if (!dlg)
                return 0;

        win_setopt(dlg, WIN_OPT_TITLE, "System Alert");
        win_setopt(dlg, WIN_OPT_RENDER, app_shutdown_render);
        win_setopt(dlg, WIN_OPT_BG, COLOR_BLUE);

        dlg->y = (LINES - h) / 2;
        dlg->x = (COLS - w) / 2;
        mvwin(dlg->ptr, dlg->y, dlg->x);

        win_vibrate();

        while (1) {
                win_refresh_all();
                int ch = wgetch(dlg->ptr);
                if (ch == 'y' || ch == 'Y') {
                        res = 1;
                        break;
                }
                if (ch == 'n' || ch == 'N' || ch == 27) {
                        res = 0;
                        break;
                }
        }

        win_destroy_focused();
        return res;
}

static void dispatch_input(int ch)
{
        cosh_win_t *f = (wm.focus_idx >= 0) ? wm.stack[wm.focus_idx] : NULL;

        if (ch == MODIFIER_KEY) {
                int next = getch();
                switch (next) {
                        /* Window Management */
                case 'f':
                case 'F':
                        if (f)
                                win_toggle_fullscreen(f);
                        return;
                case 'w':
                        win_resize_focused(1, 2);
                        return;
                case 'n':
                        win_resize_focused(-1, -2);
                        return;
                case 'q':
                case 'Q':
                        win_destroy_focused();
                        return;

                        /* Navigation & Apps */
                case 'p':
                case 'P':
                        win_spawn_palette();
                        return;

                        /* Movement */
                case 'h':
                        win_move_focused(0, -2);
                        return;
                case 'l':
                        win_move_focused(0, 2);
                        return;
                case 'k':
                        win_move_focused(-1, 0);
                        return;
                case 'j':
                        win_move_focused(1, 0);
                        return;

                        /* for unfocus */
                case ERR:
                case CTRL('a'):
                        wm.focus_idx = -1;
                        for (int i = 0; i < wm.count; i++)
                                wm.stack[i]->dirty = 1;
                        win_needs_redraw = 1;
                        return;
                }
        }

        switch (ch) {
        case KEY_MOUSE:
                win_handle_mouse();
                break;
        case '\t':
                if (wm.count > 1)
                        win_raise(0);
                break;
        default:
                if (f && f->input_cb) {
                        f->input_cb(f, ch);
                        f->dirty = 1;
                } else if (!f) {
                        win_vibrate();
                }
                break;
        }
        win_needs_redraw = 1;
}

int boot(void)
{
        struct stat sb;
	setlocale(LC_ALL, "");

        k_log_trace("ensure workdir...");

        if (lstat(WORKDIR, &sb) == -1) {
                k_log_info("workdir uninitialized: %s", strerror(errno));
                mkdir(WORKDIR, 0755);
        } else {
                if (S_ISREG(sb.st_mode)) {
                        k_log_error
                            ("workdir: %s is already exist as regular file (expected: directory)",
                             WORKDIR);
                        return -1;
                }

        }
        k_log_info("workdir is ready");

        k_log_trace("preparing for workdir state...");
        wstate = malloc(sizeof(*wstate));
        if (!wstate) {
                k_log_fatal("malloc for workdir state failed.");
                return -1;
        }
        k_log_info("workdir state ready");

        k_log_trace("preparing software...");
        register_app("Adams Terminal", win_spawn_iterm);
        register_app("Palette", win_spawn_palette);
        register_app("Guide", win_spawn_help);

        k_log_trace("rendering window...");
        wm_init();              /* Init TUI */

        return 0;
}

void shutdown(void)
{
        int dummy;

        free(wstate);
        cleanup_empty_files(WORKDIR, &dummy);
        endwin();
}

int get_workdir_usage(void)
{
        struct stat st;
        if (lstat(WORKDIR, &st) == -1)
                return -1;

        if (wstate->cached == 1 && st.st_mtime == wstate->last_mtime)
                return (int)(wstate->cached_size / 1024);

        if (!S_ISDIR(st.st_mode))
                return st.st_size;

        DIR *d = opendir(WORKDIR);
        if (!d)
                return -1;

        long total_size = 0;
        struct dirent *de;
        char path[1024];

        while ((de = readdir(d)) != NULL) {
                if (strcmp(de->d_name, ".") == 0
                    || strcmp(de->d_name, "..") == 0)
                        continue;

                snprintf(path, sizeof(path), "%s/%s", WORKDIR, de->d_name);

                struct stat fst;
                if (lstat(path, &fst) != -1) {
                        total_size += (fst.st_blocks * 512);
                }
        }

        closedir(d);
        wstate->last_mtime = st.st_mtime;
        wstate->cached_size = total_size;
        wstate->cached = true;
        return (int)(total_size / 1024);
}

int main(void)
{
        struct pollfd pfd = {.fd = 0,.events = POLLIN };
        int ch;

        boot();
        win_spawn_iterm();
        if (wm.focus_idx >= 0)
                win_toggle_fullscreen(wm.stack[wm.focus_idx]);

        win_spawn_help();

        while (1) {
                if (terminal_resized)
                        win_handle_resize();

                if (win_needs_redraw)
                        win_refresh_all();

                if (poll(&pfd, 1, TICK_DELAY) > 0) {
                        ch = getch();
                        if (ch == CTRL('/')) {
                                if (confirm_shutdown())
                                        break;
                                win_needs_redraw = 1;
                                continue;
                        }
                        dispatch_input(ch);
                } else {
                        win_needs_redraw = 1;
                }
        }

        shutdown();
        return 0;
}
