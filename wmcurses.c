#include "wmcurses.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

cosh_wm_t wm = { .count = 0, .focus_idx = -1 };
int win_needs_redraw = 1;
int win_force_full = 0;

void wm_init(void)
{
	initscr();
	raw();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	start_color();
	set_escdelay(25);
	mousemask(BUTTON1_PRESSED, NULL);

	init_pair(CP_TOS_STD, COLOR_BLUE, COLOR_WHITE);
	init_pair(CP_TOS_HDR, COLOR_WHITE, COLOR_BLUE);
	init_pair(CP_TOS_ACC, COLOR_RED, COLOR_WHITE);
	init_pair(CP_TOS_BAR, COLOR_WHITE, COLOR_BLUE);

	wbkgd(stdscr, COLOR_PAIR(CP_TOS_STD));
}

static void win_render_frame(cosh_win_t *win, int is_focused)
{
	int color = is_focused ? CP_TOS_HDR : CP_TOS_STD;

	wattron(win->ptr, COLOR_PAIR(CP_TOS_STD));
	box(win->ptr, 0, 0);

	wattron(win->ptr, COLOR_PAIR(color));
	for (int i = 1; i < win->w - 1; i++)
		mvwaddch(win->ptr, 0, i, ' ');

	mvwprintw(win->ptr, 0, 2, " %s ", win->title);
	
	wattron(win->ptr, COLOR_PAIR(CP_TOS_ACC));
	mvwprintw(win->ptr, 0, win->w - 4, "[x]");
	
	wattroff(win->ptr, COLOR_PAIR(CP_TOS_ACC));
	wattroff(win->ptr, COLOR_PAIR(color));
}

void win_raise(int idx)
{
	if (idx < 0 || idx >= wm.count || idx == wm.count - 1) {
		wm.focus_idx = (wm.count > 0) ? wm.count - 1 : -1;
		return;
	}

	cosh_win_t *tmp = wm.stack[idx];
	for (int i = idx; i < wm.count - 1; i++)
		wm.stack[i] = wm.stack[i + 1];

	wm.stack[wm.count - 1] = tmp;
	wm.focus_idx = wm.count - 1;
}

cosh_win_t *win_create(int h, int w, char *title, render_fn r, input_fn i)
{
	cosh_win_t *win;
	int ny, nx;

	if (wm.count >= WIN_MAX)
		return NULL;

	win = calloc(1, sizeof(cosh_win_t));
	if (!win)
		return NULL;

	ny = 2 + (wm.count * 2) % (LINES - h - 2);
	nx = 5 + (wm.count * 4) % (COLS - w - 2);

	win->ptr = newwin(h, w, ny, nx);
	win->x = nx; win->y = ny;
	win->w = w;  win->h = h;
	win->active = 1;
	win->dirty = 1;
	win->render_cb = r;
	win->input_cb = i;
	strncpy(win->title, title, 31);

	wbkgd(win->ptr, COLOR_PAIR(CP_TOS_STD));
	keypad(win->ptr, TRUE);

	wm.stack[wm.count] = win;
	wm.focus_idx = wm.count;
	wm.count++;

	win_needs_redraw = 1;
	return win;
}

void win_destroy_focused(void)
{
	int idx = wm.focus_idx;
	if (idx < 0 || idx >= wm.count)
		return;

	if (wm.stack[idx]->priv)
		free(wm.stack[idx]->priv);

	delwin(wm.stack[idx]->ptr);
	free(wm.stack[idx]);

	for (int i = idx; i < wm.count - 1; i++)
		wm.stack[i] = wm.stack[i + 1];

	wm.count--;
	wm.focus_idx = (wm.count > 0) ? wm.count - 1 : -1;
	win_needs_redraw = 1;
}

void win_move_focused(int dy, int dx)
{
	cosh_win_t *win;
	if (wm.focus_idx < 0)
		return;

	win = wm.stack[wm.focus_idx];
	win->y += dy;
	win->x += dx;
	mvwin(win->ptr, win->y, win->x);
	win_raise(wm.focus_idx);
	win_needs_redraw = 1;
}

void win_handle_mouse(void)
{
	MEVENT event;
	if (getmouse(&event) != OK)
		return;

	if (event.bstate & BUTTON1_PRESSED) {
		for (int i = wm.count - 1; i >= 0; i--) {
			cosh_win_t *w = wm.stack[i];
			if (event.y >= w->y && event.y < (w->y + w->h) &&
			    event.x >= w->x && event.x < (w->x + w->w)) {
				win_raise(i);
				win_needs_redraw = 1;
				return;
			}
		}

		wm.focus_idx = -1;
		win_needs_redraw = 1;
	}
}

static void draw_statusbar(void)
{
	char time_str[16];
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	
	strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

	attron(COLOR_PAIR(CP_TOS_BAR));
	mvprintw(LINES - 1, 0, "🕓 %s | Open: %d/%d | +[%s]", 
		 time_str, wm.count, WIN_MAX, wm.focus_idx >= 0 ? wm.stack[wm.focus_idx]->title : "Desktop");
	clrtoeol();
	attroff(COLOR_PAIR(CP_TOS_BAR));
}

void win_refresh_all(void)
{
	if (win_force_full) {
		clearok(stdscr, TRUE);
		win_force_full = 0;
	}

	werase(stdscr);
	draw_statusbar();
	wnoutrefresh(stdscr);

	for (int i = 0; i < wm.count; i++) {
		cosh_win_t *w = wm.stack[i];
		if (w->dirty) {
			werase(w->ptr);
			if (w->render_cb)
				w->render_cb(w);
			w->dirty = 0;
		}
		if (win_needs_redraw)
			win_render_frame(w, (i == wm.focus_idx));
		wnoutrefresh(w->ptr);
	}

	doupdate();
	win_needs_redraw = 0;
}
