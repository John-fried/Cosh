#include "../wmcurses.h"
#include "../cosh.h"

void app_shutdown_render(cosh_win_t *win)
{
	mvwprintw(win->ptr, 2, (win->w - 16) / 2, "CONFIRM SHUTDOWN");
	mvwprintw(win->ptr, 3, (win->w - 22) / 2, "Press [y] Yes | [n] No");
}

void confirm_shutdown(void)
{
	int h = 6; int w = 40;
	cosh_win_t *dlg = win_create(h, w, WIN_FLAG_LOCKED);
	if (!dlg)
		return;

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
			c_shutdown();
			break;
		}
		if (ch == 'n' || ch == 'N' || ch == 27) {
			break;
		}
	}

	win_destroy_focused();
}


