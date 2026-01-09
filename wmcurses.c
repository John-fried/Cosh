#include "wmcurses.h"
#include "cosh.h"

/* Mouse Wheel definitions */
#ifndef BUTTON4_PRESSED
#define BUTTON4_PRESSED  000020000000L
#endif
#ifndef BUTTON5_PRESSED
#define BUTTON5_PRESSED  000200000000L
#endif

/* Mouse Wheel definitions for Horizontal Scroll */
#ifndef BUTTON6_PRESSED
#define BUTTON6_PRESSED  000400000000L
#endif
#ifndef BUTTON7_PRESSED
#define BUTTON7_PRESSED  001000000000L
#endif

cosh_wm_t wm = {.count = 0,.focus_idx = -1 };

int win_needs_redraw = 1;
int win_force_full = 0;
volatile sig_atomic_t terminal_resized = 0;

void handle_sigwinch(int sig)
{
        (void)sig;
        terminal_resized = 1;
}

/**
 * win_apply_colors - Assign and initialize color pairs for a window
 */
static void win_apply_colors(cosh_win_t *win, int idx)
{
        if (!win || !win->ptr)
                return;

        if (win->fg != -1 && win->bg != -1) {
                win->color_pair = CP_WIN_START + idx;
                init_pair(win->color_pair, win->fg, win->bg);
        } else {
                win->color_pair = CP_TOS_STD;
        }

        wbkgd(win->ptr, COLOR_PAIR(win->color_pair));
}

/**
 * wm_init - Initialize terminal and color system
 */
void wm_init(void)
{
        initscr();
        raw();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
        start_color();
        set_escdelay(25);

        signal(SIGWINCH, handle_sigwinch);

        if (can_change_color())
                init_color(COLOR_BLUE, 0, 0, 666);

        mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED |
                  BUTTON4_PRESSED | BUTTON5_PRESSED |
                  BUTTON6_PRESSED | BUTTON7_PRESSED, NULL);

        init_pair(CP_TOS_STD, COLOR_BLUE, COLOR_WHITE);
        init_pair(CP_TOS_HDR, COLOR_WHITE, COLOR_BLUE);
        init_pair(CP_TOS_ACC, COLOR_RED, COLOR_WHITE);
        init_pair(CP_TOS_BAR, COLOR_WHITE, COLOR_BLUE);
        init_pair(CP_TOS_HDR_UNF, COLOR_WHITE, COLOR_GREY);

        wbkgd(stdscr, COLOR_PAIR(CP_TOS_STD));
}

/**
 * win_create - Create window and initialize its panel
 */
cosh_win_t *win_create(int h, int w, int flags)
{
        cosh_win_t *win;
        int ny, nx;

        if (wm.count >= WIN_MAX)
                return NULL;

        if (h <= 0)
                h = LINES / 2;
        if (w <= 0)
                w = COLS / 2;

        if (h > LINES - 1)
                h = LINES - 1;
        if (w > COLS)
                w = COLS;

        win = calloc(1, sizeof(cosh_win_t));
        if (!win)
                return NULL;

        ny = 2 + (wm.count * 2) % (LINES - h - 1);
        nx = 5 + (wm.count * 4) % (COLS - w - 1);

        win->ptr = newwin(h, w, ny, nx);
        win->panel = new_panel(win->ptr);
        win->x = nx;
        win->y = ny;
        win->w = w;
        win->h = h;
        win->vw = w - 4;
        win->vh = h - 2;
        win->flags = flags;
        win->dirty = 1;
        win->fg = -1;
        win->bg = -1;

        keypad(win->ptr, TRUE);
        scrollok(win->ptr, TRUE);
        idlok(win->ptr, TRUE);

        wm.stack[wm.count] = win;
        win_apply_colors(win, wm.count);

        wm.focus_idx = wm.count;
        wm.count++;

        win_needs_redraw = 1;
        return win;
}

/**
 * win_resize_focused - Resize the current focused window
 */
void win_resize_focused(int dh, int dw)
{
        cosh_win_t *w;
        if (wm.focus_idx < 0)
                return;

        w = wm.stack[wm.focus_idx];
        if (w->flags & (WIN_FLAG_LOCKED | WIN_FLAG_FULLSCREEN))
                return;

        int nh = w->h + dh;
        int nw = w->w + dw;

        if (nh < 4 || nh > LINES - 1)
                return;
        if (nw < 10 || nw > COLS)
                return;

        w->h = nh;
        w->w = nw;
        w->vw = nw - 4;
        w->vh = nh - 2;

        wresize(w->ptr, w->h, w->w);
        replace_panel(w->panel, w->ptr);

        if (w->resize_cb)
                w->resize_cb(w, w->vh, w->vw);

        w->dirty = 1;
        win_needs_redraw = 1;
}

void win_handle_resize(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
		resizeterm(ws.ws_row, ws.ws_col);
	else
		resize_term(0, 0);

	getmaxyx(stdscr, LINES, COLS);

        terminal_resized = 0;
        win_force_full = 1;

        for (int i = 0; i < wm.count; i++) {
                cosh_win_t *w = wm.stack[i];

                if (w->flags & WIN_FLAG_FULLSCREEN) {
                        w->x = 0;
                        w->y = 0;
                        w->w = COLS;
                        w->h = LINES - 1;       // little space for statusbar
                } else {
                        // Clamp the position so that the bar does not go out of the screen
                        if (w->y + w->h > LINES - 1)
                                w->y =
                                    (LINES - 1 - w->h >
                                     0) ? LINES - 1 - w->h : 0;
                        if (w->x + w->w > COLS)
                                w->x = (COLS - w->w > 0) ? COLS - w->w : 0;
                }

                w->vw = w->w - 4;
                w->vh = w->h - 2;

                wresize(w->ptr, w->h, w->w);
                replace_panel(w->panel, w->ptr);
                move_panel(w->panel, w->y, w->x);

                if (w->resize_cb)
                        w->resize_cb(w, w->vh, w->vw);

                w->dirty = 1;
        }
}

/**
 * win_setopt - Set window options/callbacks
 */
void win_setopt(cosh_win_t *win, win_opt_t opt, ...)
{
        va_list ap;
        int idx = -1;

        if (!win)
                return;

        for (int i = 0; i < wm.count; i++) {
                if (wm.stack[i] == win) {
                        idx = i;
                        break;
                }
        }

        va_start(ap, opt);
        switch (opt) {
        case WIN_OPT_TITLE:
                strncpy(win->title, va_arg(ap, char *), 31);
                break;
        case WIN_OPT_RENDER:
                win->render_cb = va_arg(ap, render_fn);
                break;
        case WIN_OPT_INPUT:
                win->input_cb = va_arg(ap, input_fn);
                break;
        case WIN_OPT_FG:
                win->fg = va_arg(ap, int);
                if (idx != -1)
                        win_apply_colors(win, idx);
                break;
        case WIN_OPT_BG:
                win->bg = va_arg(ap, int);
                if (idx != -1)
                        win_apply_colors(win, idx);
                break;
        case WIN_OPT_PRIV:
                win->priv = va_arg(ap, void *);
                break;
        case WIN_OPT_DESTROY:
                win->destroy_cb = va_arg(ap, destroy_fn);
                break;
        case WIN_OPT_TICK:
                win->tick_cb = va_arg(ap, tick_fn);
                break;
        case WIN_OPT_RESIZE:
                win->resize_cb = va_arg(ap, resize_fn);
                break;
        }
        va_end(ap);
        win->dirty = 1;
}

/**
 * win_render_frame - Draw borders and title
 */
static void win_render_frame(cosh_win_t *win, int is_focused)
{
        int hdr_color = is_focused ? CP_TOS_HDR : CP_TOS_HDR_UNF;

        wattron(win->ptr, COLOR_PAIR(win->color_pair));
        box(win->ptr, 0, 0);
        wattron(win->ptr, COLOR_PAIR(hdr_color));

        for (int i = 1; i < win->w - 1; i++)
                mvwaddch(win->ptr, 0, i, ' ');

        mvwprintw(win->ptr, 0, 2, " %s ", win->title);

        if (!(win->flags & WIN_FLAG_LOCKED)) {
                wattron(win->ptr, COLOR_PAIR(CP_TOS_ACC));
                mvwprintw(win->ptr, 0, win->w - 4, "X");
                wattroff(win->ptr, COLOR_PAIR(CP_TOS_ACC));
        }

        wattroff(win->ptr, COLOR_PAIR(hdr_color));
}

/**
 * win_raise - Raise window to top using panels
 */
void win_raise(int idx)
{
        cosh_win_t *tmp;
        if (idx < 0 || idx >= wm.count)
                return;

        if (idx != wm.count - 1) {
                tmp = wm.stack[idx];
                for (int i = idx; i < wm.count - 1; i++)
                        wm.stack[i] = wm.stack[i + 1];
                wm.stack[wm.count - 1] = tmp;
        }

        wm.focus_idx = wm.count - 1;
        top_panel(wm.stack[wm.focus_idx]->panel);

        for (int i = 0; i < wm.count; i++)
                wm.stack[i]->dirty = 1;
}

/**
 * win_vibrate - Shake effect for visual alert
 */
void win_vibrate(void)
{
        beep();
        if (wm.focus_idx >= 0) {
                cosh_win_t *w = wm.stack[wm.focus_idx];
                int orig_x = w->x;
                int offsets[] = { -1, 1, -1, 1, 0 };

                for (int i = 0; i < 5; i++) {
                        w->x = orig_x + offsets[i];
                        move_panel(w->panel, w->y, w->x);
                        win_refresh_all();
                        usleep(30000);
                }
        }
}

void win_ding(void)
{
        win_vibrate();
}

/**
 * win_toggle_fullscreen - Maximize/Restore window
 */
void win_toggle_fullscreen(cosh_win_t *win)
{
        if (!win)
                return;

        if (!(win->flags & WIN_FLAG_FULLSCREEN)) {
                win->rx = win->x;
                win->ry = win->y;
                win->rw = win->w;
                win->rh = win->h;

                win->x = 0;
                win->y = 0;
                win->w = COLS;
                win->h = LINES - 1;
                win->flags |= WIN_FLAG_FULLSCREEN;
        } else {
                win->x = win->rx;
                win->y = win->ry;
                win->w = win->rw;
                win->h = win->rh;
                win->flags &= ~WIN_FLAG_FULLSCREEN;
        }

        wresize(win->ptr, win->h, win->w);
        replace_panel(win->panel, win->ptr);
        move_panel(win->panel, win->y, win->x);

        win->dirty = 1;
        win_needs_redraw = 1;
}

/**
 * win_destroy_focused - Clean up current window and its panel
 */
void win_destroy_focused(void)
{
        int idx = wm.focus_idx;
        if (idx < 0 || idx >= wm.count)
                return;

        if (wm.stack[idx]->destroy_cb)
                wm.stack[idx]->destroy_cb(wm.stack[idx]->priv);
        else if (wm.stack[idx]->priv)
                free(wm.stack[idx]->priv);

        del_panel(wm.stack[idx]->panel);
        delwin(wm.stack[idx]->ptr);
        free(wm.stack[idx]);

        for (int i = idx; i < wm.count - 1; i++)
                wm.stack[i] = wm.stack[i + 1];

        wm.count--;
        wm.focus_idx = (wm.count > 0) ? wm.count - 1 : -1;

        if (wm.focus_idx >= 0)
                top_panel(wm.stack[wm.focus_idx]->panel);

        win_needs_redraw = 1;
}

/**
 * win_move_focused - Move window using panel's move
 */
void win_move_focused(int dy, int dx)
{
        cosh_win_t *w;
        if (wm.focus_idx < 0)
                return;

        w = wm.stack[wm.focus_idx];
        if (w->flags & (WIN_FLAG_LOCKED | WIN_FLAG_FULLSCREEN))
                return;

        w->y += dy;
        w->x += dx;

        if (w->y < 0)
                w->y = 0;
        if (w->x < 0)
                w->x = 0;
        if (w->y > LINES - 2)
                w->y = LINES - 2;
        if (w->x > COLS - 2)
                w->x = COLS - 2;

        move_panel(w->panel, w->y, w->x);
        win_needs_redraw = 1;
}

/**
 * win_handle_mouse - Process mouse events
 */
void win_handle_mouse(void)
{
        MEVENT ev;
        if (getmouse(&ev) != OK)
                return;

        for (int i = wm.count - 1; i >= 0; i--) {
                cosh_win_t *w = wm.stack[i];

                if (ev.y >= w->y && ev.y < (w->y + w->h) &&
                    ev.x >= w->x && ev.x < (w->x + w->w)) {

                        win_raise(i);
                        w->last_seq = WIN_SEQ_NONE;

                        if (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                                if (!(w->flags & WIN_FLAG_LOCKED)
                                    && ev.y == w->y && ev.x >= (w->x + w->w - 4)
                                    && ev.x < (w->x + w->w - 1)) {
                                        win_destroy_focused();
                                        return;
                                }
                                if (w->input_cb)
                                        w->input_cb(w, KEY_MOUSE);
                        } else if (ev.bstate & BUTTON4_PRESSED) {
                                w->last_seq = WIN_MOUSE_SCROLL_UP;
                                if (w->input_cb)
                                        w->input_cb(w, (int)w->last_seq);
                        } else if (ev.bstate & BUTTON5_PRESSED) {
                                w->last_seq = WIN_MOUSE_SCROLL_DOWN;
                                if (w->input_cb)
                                        w->input_cb(w, (int)w->last_seq);
                        }

                        win_needs_redraw = 1;
                        return;
                }
        }

        if (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                wm.focus_idx = -1;
                win_needs_redraw = 1;
        }
}

/**
 * draw_statusbar - Render the bottom info bar
 */
static void draw_statusbar(void)
{
        char time_str[16];
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

        attron(COLOR_PAIR(CP_TOS_BAR));
        mvprintw(LINES - 1, 0, " %s | Used: %d | Open: %d | [%s]",
                 time_str, get_workdir_usage(), wm.count,
                 wm.focus_idx >= 0 ? wm.stack[wm.focus_idx]->title : "Desktop");
        clrtoeol();
        attroff(COLOR_PAIR(CP_TOS_BAR));
}

/**
 * win_refresh_all - Core redraw loop using panel updates
 */
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
                if (w->tick_cb)
                        w->tick_cb(w);

                if (w->dirty) {
                        werase(w->ptr);
                        if (w->render_cb)
                                w->render_cb(w);
                        w->dirty = 0;
                }
                win_render_frame(w, (i == wm.focus_idx));
        }

        update_panels();
        doupdate();
        win_needs_redraw = 0;
}
