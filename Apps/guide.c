#include "../wmcurses.h"
#include "../cosh.h"

void app_help_render(cosh_win_t *win)
{
        mvwprintw(win->ptr, 2, 2, "--- SHORTCUTS ---");

        /* Window Management */
        mvwprintw(win->ptr, 4, 2, "ALT + F         : Toggle Fullscreen");
        mvwprintw(win->ptr, 5, 2, "ALT + Q         : Close Window");
        mvwprintw(win->ptr, 6, 2, "ALT + W / N     : Resize (Grow/Shrink)");

        /* Movement */
        mvwprintw(win->ptr, 8, 2, "ALT + H,J,K,L   : Move Window");

        /* Navigation & System */
        mvwprintw(win->ptr, 10, 2, "ALT + P         : Apps Palette");
        mvwprintw(win->ptr, 11, 2, "Tab             : Cycle Focus");
        mvwprintw(win->ptr, 12, 2, "ALT + CTRL + a  : Unfocus All");
        mvwprintw(win->ptr, 13, 2, "Ctrl + /        : Shutdown System");
}

void win_spawn_help(void)
{
        cosh_win_t *win = win_create(16, 55, WIN_FLAG_NONE);
        if (!win)
                return;

        win_setopt(win, WIN_OPT_TITLE, "Quick Help");
        win_setopt(win, WIN_OPT_RENDER, app_help_render);
}
