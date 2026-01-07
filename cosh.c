#include "wmcurses.h"
#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

/* Forward decl for app spawner */
void win_spawn_iterm(void);

void app_help_render(cosh_win_t *win)
{
	mvwprintw(win->ptr, 2, 2, "--- SHORTCUTS ---");
	mvwprintw(win->ptr, 4, 2, "Arrows : Move Window");
	mvwprintw(win->ptr, 5, 2, "Esc    : Unfocus all window");
	mvwprintw(win->ptr, 6, 2, "Tab    : Cycle Focus");
	mvwprintw(win->ptr, 7, 2, "Ctrl+P : Apps pallete");
	mvwprintw(win->ptr, 8, 2, "Ctrl+G : Show this guide");
	mvwprintw(win->ptr, 9, 2, "q      : Close Window");
	mvwprintw(win->ptr, 10, 2, "Q      : Shutdown");
}

int confirm_shutdown(void)
{
	int h = 5, w = 40;
	int y = (LINES - h) / 2;
	int x = (COLS - w) / 2;
	int res = 0;
	WINDOW *dlg = newwin(h, w, y, x);

	/* Gunakan warna Header agar terlihat seperti modal penting */
	wbkgd(dlg, COLOR_PAIR(CP_TOS_HDR));
	box(dlg, 0, 0);
	mvwprintw(dlg, 1, (w - 16) / 2, "CONFIRM SHUTDOWN");
	mvwprintw(dlg, 3, (w - 22) / 2, "Press [y] Yes | [n] No");
	wrefresh(dlg);

	/* Loop terbatas hanya untuk y/n */
	while (1) {
		int ch = wgetch(dlg);
		if (ch == 'y' || ch == 'Y') {
			res = 1;
			break;
		}
		if (ch == 'n' || ch == 'N' || ch == 27) { /* 27 = ESC */
			res = 0;
			break;
		}
	}

	delwin(dlg);
	return res;
}

static void dispatch_input(int ch)
{
	switch (ch) {
	case 27: /* ESC */
		wm.focus_idx = -1;
		win_needs_redraw = 1;
		break;
	case KEY_MOUSE:
		win_handle_mouse();
		break;
	case '\t':
		if (wm.count > 1) win_raise(0);
		break;
	case CTRL('g'):
		win_create(12, 50, "Quick Help", app_help_render, NULL);
		break;
	case CTRL('t'):
		win_spawn_iterm();
		break;
	case 'q':
		win_destroy_focused();
		break;
	case KEY_UP:    win_move_focused(-1, 0); break;
	case KEY_DOWN:  win_move_focused(1, 0);  break;
	case KEY_LEFT:  win_move_focused(0, -2); break;
	case KEY_RIGHT: win_move_focused(0, 2);  break;
	default:
		if (wm.focus_idx >= 0) {
			cosh_win_t *f = wm.stack[wm.focus_idx];
			if (f->input_cb) {
				f->input_cb(f, ch);
				f->dirty = 1;
			}
		}
		break;
	}
	win_needs_redraw = 1;
}

int main(void)
{
	struct pollfd pfd = { .fd = 0, .events = POLLIN };

	mkdir(".cosh", 0755);
	wm_init();

	/* Initial App */
	win_spawn_iterm();

	while (1) {
		struct timeval tv;
		int timeout;

		if (win_needs_redraw)
			win_refresh_all();

		gettimeofday(&tv, NULL);
		timeout = 1000 - (tv.tv_usec / 1000);

		if (poll(&pfd, 1, timeout) > 0) {
			int ch = getch();
			if (ch == 'Q')
				if (confirm_shutdown())
					break;
				else
					win_needs_redraw = 1;

			dispatch_input(ch);
		} else {
			/* Heartbeat/Clock update */
			win_needs_redraw = 1;
		}
	}

	endwin();
	return 0;
}
