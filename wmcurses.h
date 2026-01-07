#ifndef WMCURSES_H
#define WMCURSES_H

#include <ncurses.h>

#define WIN_MAX		32
#define CTRL(c)		((c) & 037)

/* TOS-inspired Color Pairs */
#define CP_TOS_STD	1	/* Blue on White */
#define CP_TOS_HDR	2	/* White on Blue (Focused) */
#define CP_TOS_ACC	3	/* Red on White (Widgets) */
#define CP_TOS_BAR	4	/* White on Blue (Status) */

struct cosh_win;

typedef void (*render_fn)(struct cosh_win *win);
typedef void (*input_fn)(struct cosh_win *win, int ch);

typedef struct cosh_win {
	WINDOW *ptr;
	char title[32];
	int x, y, w, h;
	int active;
	int dirty;
	void *priv;		/* App-specific state */
	render_fn render_cb;
	input_fn input_cb;
} cosh_win_t;

typedef struct {
	cosh_win_t *stack[WIN_MAX];
	int count;
	int focus_idx;
} cosh_wm_t;

extern cosh_wm_t wm;
extern int win_needs_redraw;
extern int win_force_full;

void wm_init(void);
cosh_win_t *win_create(int h, int w, char *title, render_fn r, input_fn i);
void win_destroy_focused(void);
void win_raise(int idx);
void win_move_focused(int dy, int dx);
void win_handle_mouse(void);
void win_refresh_all(void);

#endif /* WMCURSES_H */
