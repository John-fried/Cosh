#include "cosh.h"

void win_spawn_iterm(void);
void win_spawn_palette(void);
void win_spawn_help(void);

app_entry_t *app_registry = NULL;
int app_count = 0;

void register_app(const char *name, void (*spawn)(void))
{
        app_entry_t *new_app = malloc(sizeof(app_entry_t));

	if (!new_app) {
		k_log_error("Failed to load software '%s': %s", name, strerror(errno));
		return;
	}

        new_app->name = strdup(name);
        new_app->spawn = spawn;
        new_app->score = 0;
        new_app->next = app_registry;
        app_registry = new_app;
        app_count++;

}

void app_shutdown_render(cosh_win_t *win)
{
        mvwprintw(win->ptr, 2, (win->w - 16) / 2, "CONFIRM SHUTDOWN");
        mvwprintw(win->ptr, 3, (win->w - 22) / 2, "Press [y] Yes | [n] No");
}

int confirm_shutdown(void)
{
        int h = 6, w = 40, res = 0;
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
        if (k_boot() != 0)
                return -1;

        k_log_trace("Preparing software...");
        register_app("Adams Terminal", win_spawn_iterm);
        register_app("Palette", win_spawn_palette);
        register_app("Guide", win_spawn_help);

	/* start the kernel */
	k_start();

        return 0;
}

int main(void)
{
        struct pollfd pfd = {.fd = 0,.events = POLLIN };
        int ch;

        if (boot() != 0) {
                k_log_fatal("some of booting process was failed. Aborting.");
                return 1;
        }

        win_spawn_iterm();
        win_toggle_fullscreen(wm.stack[wm.focus_idx]);
        win_spawn_help();

        while (1) {
                if (terminal_resized)
                        win_handle_resize();

		for (int i = 0; i < wm.count; i++)
			if (wm.stack[i]->tick_cb)
				wm.stack[i]->tick_cb(wm.stack[i]);

                if (win_needs_redraw)
                        win_refresh_all();

                if (poll(&pfd, 1, TICK_DELAY) > 0) {
                        while ((ch = getch()) != ERR) {
				if (ch == CTRL('/')) {
					if (confirm_shutdown())
						goto shutdown;
					win_needs_redraw = 1;
					continue;
				}
				dispatch_input(ch);
			}
                }
	}
	
	return 0;

shutdown:
        k_shutdown();
        return 0;
}
