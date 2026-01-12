#ifndef WMCURSES_H
#define WMCURSES_H

#define _XOPEN_SOURCE_EXTENDED
#include <curses.h>
#include <ncurses.h>
#include <ncurses/panel.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define WIN_MAX		32
#define CTRL(c)		(((c) ^ ((((c) ^ 0x40) >> 2) & 0x10)) & 0x1f)

/* Window Flags for win_create */
#define WIN_FLAG_NONE		0x00
#define WIN_FLAG_LOCKED		0x01    /* Position cannot be moved */
#define WIN_FLAG_FULLSCREEN	0x02    /* Window is in fullscreen mode */

/* Options for win_setopt */
typedef enum {
        WIN_OPT_TITLE = 1,
        WIN_OPT_RENDER = 2,
        WIN_OPT_INPUT = 3,
        WIN_OPT_FG = 4,
        WIN_OPT_BG = 5,
        WIN_OPT_PRIV = 6,
        WIN_OPT_DESTROY = 7,
        WIN_OPT_TICK = 8,
        WIN_OPT_RESIZE = 9,
        WIN_OPT_CURSOR = 10
} win_opt_t;

typedef enum {
        WIN_SEQ_NONE = 0,
        WIN_MOUSE_SCROLL_UP = 0x2001,
        WIN_MOUSE_SCROLL_DOWN = 0x2002,
        WIN_MOUSE_SCROLL_LEFT = 0x2003,
        WIN_MOUSE_SCROLL_RIGHT = 0x2004
} win_seq_t;

typedef enum {
        WIN_NOTHING = 0,
        WIN_DONT_RESIZE = 1,    /* Dont auto resize buffer, usefull if you have a window which is aimed directly at fullscreen */
} win_buffreq_t;

#ifndef COLOR_GREY
#define COLOR_GREY 8
#endif

#define CP_TOS_STD	1
#define CP_TOS_HDR	2
#define CP_TOS_ACC	3
#define CP_TOS_BAR	4
#define CP_TOS_HDR_UNF	5
#define CP_CURSOR	6
#define CP_WIN_BG	7

#define CP_WIN_START	20      /* Where the start index for every window color pair */

struct cosh_win;

typedef void (*render_fn)(struct cosh_win * win);
typedef void (*destroy_fn)(void *priv);
typedef void (*input_fn)(struct cosh_win * win, int ch);
typedef void (*resize_fn)(struct cosh_win * win, int new_h, int new_w);
typedef void (*tick_fn)(struct cosh_win * win);

typedef struct cosh_win {
        WINDOW *ptr;
        PANEL *panel;           /* ncurses panel integration */
        char title[32];
        int x, y, w, h;
        int vw, vh;
        int rx, ry, rw, rh;     /* Restoration coordinates */
        int active;
        int dirty;
        int color_pair;
        int flags;
        int fg, bg;             /* Cached colors */
        int show_cursor;
        int scroll_max;
        int scroll_cur;
        int cursor_y, cursor_x;
        void *priv;
        destroy_fn destroy_cb;
        render_fn render_cb;
        input_fn input_cb;
        resize_fn resize_cb;
        win_seq_t last_seq;
        tick_fn tick_cb;
} cosh_win_t;

typedef struct {
        cosh_win_t *stack[WIN_MAX];
        int count;
        int focus_idx;
} cosh_wm_t;

extern cosh_wm_t wm;
extern win_buffreq_t win_buffer_request;
extern int win_needs_redraw;
extern int win_force_full;
extern volatile sig_atomic_t terminal_resized;

void wm_init(void);

cosh_win_t *win_create(int h, int w, int flags);
void win_setopt(cosh_win_t * win, win_opt_t opt, ...);

void win_destroy_focused(void);
void win_raise(int idx);
void win_vibrate(void);
void win_toggle_fullscreen(cosh_win_t * win);
void win_resize_focused(int dh, int dw);
void win_handle_resize(void);
void win_ding(void);
void win_move_focused(int dy, int dx);
void win_handle_mouse(void);
void win_refresh_all(void);

void win_printf(cosh_win_t * win, const char *fmt, ...);
void win_attron(cosh_win_t * win, int pair);
void win_attroff(cosh_win_t * win, int pair);
void win_move_cursor(cosh_win_t * win, int y, int x);
void win_clear(cosh_win_t * win);

#endif                          /* WMCURSES_H */
