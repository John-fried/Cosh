#include "wmcurses.h"
#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

void win_spawn_iterm(void);
void win_spawn_palette(void);

void app_help_render(cosh_win_t *win)
{
	mvwprintw(win->ptr, 2, 2, "--- COSH SYSTEM SHORTCUTS ---");
	wattron(win->ptr, A_BOLD);
	mvwprintw(win->ptr, 4, 2, "Ctrl + W,A,S,D : Move Window");
	wattroff(win->ptr, A_BOLD);
	mvwprintw(win->ptr, 5, 2, "Ctrl + P       : Apps Palette");
	mvwprintw(win->ptr, 6, 2, "Ctrl + G       : Show Guide");
	mvwprintw(win->ptr, 7, 2, "Tab            : Cycle Focus");
	mvwprintw(win->ptr, 8, 2, "Esc            : Unfocus All");
	mvwprintw(win->ptr, 9, 2, "Ctrl + Q       : Close Window");
	mvwprintw(win->ptr, 10, 2, "Q              : System Shutdown");
}

void win_spawn_help(void)
{
	win_create(12, 55, "Quick Help", app_help_render, NULL);
}

int confirm_shutdown(void)
{
	int h = 5, w = 40;
	int y = (LINES - h) / 2;
	int x = (COLS - w) / 2;
	int res = 0;
	WINDOW *dlg = newwin(h, w, y, x);

	wbkgd(dlg, COLOR_PAIR(CP_TOS_HDR));
	box(dlg, 0, 0);
	mvwprintw(dlg, 1, (w - 16) / 2, "CONFIRM SHUTDOWN");
	mvwprintw(dlg, 3, (w - 22) / 2, "Press [y] Yes | [n] No");
	wrefresh(dlg);

	while (1) {
		int ch = wgetch(dlg);
		if (ch == 'y' || ch == 'Y') {
			res = 1;
			break;
		}
		if (ch == 'n' || ch == 'N' || ch == 27) {
			res = 0;
			break;
		}
	}

	delwin(dlg);
	return res;
}

static void dispatch_input(int ch)
{
	cosh_win_t *f;

	switch (ch) {
	case 27:
		wm.focus_idx = -1;
		for (int i = 0; i < wm.count; i++) wm.stack[i]->dirty = 1;
		break;
	case KEY_MOUSE:
		win_handle_mouse();
		break;
	case '\t':
		if (wm.count > 1)
			win_raise(0);
		break;
	case CTRL('p'):
		win_spawn_palette();
		break;
	case CTRL('g'):
		win_spawn_help();
		break;
	/* Primary Movement Controls */
	case CTRL('w'): win_move_focused(-1, 0); break;
	case CTRL('s'): win_move_focused(1, 0);  break;
	case CTRL('a'): win_move_focused(0, -2); break;
	case CTRL('d'): win_move_focused(0, 2);  break;
	case CTRL('q'):
		win_destroy_focused();
		break;
	default:
		if (wm.focus_idx >= 0) {
			f = wm.stack[wm.focus_idx];
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
	struct timeval tv;
	int ch, timeout;

	mkdir(".cosh", 0755);
	wm_init();

	win_spawn_iterm();

	while (1) {
		if (win_needs_redraw)
			win_refresh_all();

		gettimeofday(&tv, NULL);
		timeout = 1000 - (tv.tv_usec / 1000);

		if (poll(&pfd, 1, timeout) > 0) {
			ch = getch();
			if (ch == 'Q') {
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

	endwin();
	return 0;
}
