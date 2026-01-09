#include "../wmcurses.h"
#include "../cosh.h"


void app_help_render(cosh_win_t *win)
{
        mvwprintw(win->ptr, 2, 2, "--- COSH SYSTEM SHORTCUTS ---");
        mvwprintw(win->ptr, 4, 2, "ALT + F        : Toggle fullscreen");
        mvwprintw(win->ptr, 5, 2, "ALT + W / N    : Resize Window");
        mvwprintw(win->ptr, 6, 2, "Ctrl + W,A,S,D : Move Window");
        mvwprintw(win->ptr, 7, 2, "Ctrl + P       : Apps Palette");
        mvwprintw(win->ptr, 8, 2, "Tab            : Cycle Focus");
        mvwprintw(win->ptr, 9, 2, "Esc            : Unfocus All");
        mvwprintw(win->ptr, 10, 2, "Ctrl + q       : Close Window");
        mvwprintw(win->ptr, 11, 2, "Ctrl + /       : Shutdown");
}

void win_spawn_help(void)
{
        cosh_win_t *win = win_create(12, 55, WIN_FLAG_NONE);
        if (!win)
                return;

        win_setopt(win, WIN_OPT_TITLE, "Quick Help");
        win_setopt(win, WIN_OPT_RENDER, app_help_render);
}

