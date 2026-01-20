#include "cosh.h"

void win_spawn_iterm(void);
void win_spawn_palette(void);
void win_spawn_help(void);
void app_shutdown_render(void);
void confirm_shutdown(void);

app_entry_t *app_registry = NULL;
int app_count = 0;

void register_app(const char *name, void (*spawn)(void))
{
	app_entry_t *new_app = malloc(sizeof(app_entry_t));

	if (!new_app) {
		c_log_error("Failed to load software '%s': %s", name,
			    strerror(errno));
		return;
	}

	new_app->name = strdup(name);
	new_app->spawn = spawn;
	new_app->score = 0;
	new_app->next = app_registry;
	app_registry = new_app;
	app_count++;

}

static void dispatch_input(int ch)
{
	cosh_win_t *f = (wm.focus_idx >= 0) ? wm.stack[wm.focus_idx] : NULL;
	cfg_keys_t keyconfig = wm.configs.keys;

	if (ch == wm.configs.keys.modifier) {
		int next = getch();

		if (next == keyconfig.win_mv_left[0]) {
			win_move_focused(0, -2);
			return;
		}

		if (next == keyconfig.win_mv_right[0]) {
			win_move_focused(0, 2);
			return;
		}

		if (next == keyconfig.win_mv_up[0]) {
			win_move_focused(-1, 0);
			return;
		}

		if (next == keyconfig.win_mv_down[0]) {
			win_move_focused(1, 0);
			return;
		}

		if (next == keyconfig.tog_fullscr[0]) {
			if (f)
				win_toggle_fullscreen(f);
			else
				beep();
			return;
		}

		switch (next) {
			/* Window Management */

			// cycle
		case CTRL('n'):
		case '\t':
			win_raise(0);
			break;
		case CTRL('p'):
			if (wm.count > 1)
				win_raise(wm.count - 2);
			break;

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

			/* for unfocus */
		case CTRL('a'):
			wm.focus_idx = -1;
			for (int i = 0; i < wm.count; i++)
				wm.stack[i]->dirty = 1;
			win_needs_redraw = 1;
			return;
		case CTRL('b'):
			wm.configs.show_border = !wm.configs.show_border;
			break;
		}
	}

	switch (ch) {
		case KEY_MOUSE:
			win_handle_mouse();
			break;
		default:
			if (f && f->input_cb) {
				f->input_cb(f, ch, NULL);
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
	if (c_boot() != 0)
		return -1;

	c_log_trace("Preparing software...");
	register_app("Terminal", win_spawn_iterm);
	register_app("Palette", win_spawn_palette);
	register_app("Guide", win_spawn_help);
	register_app("Shutdown", confirm_shutdown);

	c_start();

	return 0;
}

int main(void)
{
	struct pollfd pfds[WIN_MAX + 1];
	int ch;

	if (boot() != 0) {
		c_log_fatal("some of booting process was failed. Aborting.");
		return 1;
	}

	win_spawn_iterm();
	win_toggle_fullscreen(wm.stack[wm.focus_idx]);
	win_spawn_help();

	while (1) {
		if (terminal_resized)
			win_handle_resize();

		pfds[0].fd = STDIN_FILENO;
		pfds[0].events = POLLIN;
		int nfds = 1;

		for (int i = 0; i < wm.count; i++) {
			if (wm.stack[i]->poll_fd >= 0) {
				pfds[nfds].fd = wm.stack[i]->poll_fd;
				pfds[nfds].events = POLLIN;
				nfds++;
			}
		}

		//check for ms
		int ret = poll(pfds, nfds, wm.configs.desktop.refresh_rate);

		if (ret > 0) {
			//Keyboard (pfds[0])
			if (pfds[0].revents & POLLIN) {
				while ((ch = getch()) != ERR) {
					if (ch == CTRL('/')) {
						confirm_shutdown();
					} else {
						dispatch_input(ch);
					}
				}
			}
			// app FD (pfds[1...nfds])
			int current_pfd = 1;
			for (int i = 0; i < wm.count; i++) {
				if (wm.stack[i]->poll_fd >= 0) {
					if (pfds[current_pfd].revents &
					    (POLLIN | POLLHUP | POLLERR)) {
						if (wm.stack[i]->tick_cb)
							wm.stack[i]->
							    tick_cb(wm.stack
								    [i]);
					}
					current_pfd++;
				}
			}

			win_needs_redraw = 1;
		}

		if (win_needs_redraw) win_refresh_all();
	}

	return 0;
}
