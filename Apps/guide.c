#include "../wmcurses.h"
#include "../cosh.h"

static const struct {
	char key[60];
	char description[256];
} key_shortcut[] = {
	{"ALT + F", "Toggle fullscreen"},
	{"ALT + Q", "Close window"},
	{"ALT + W / N", "Increase/Decrease window size"},
	{"ALT + H,J,K,L", "Move window"},
	{"ALT + P", "Open Apps pallete"},
	{"ALT + TAB / ^N", "Cycle focus to next window"},
	{"ALT + ^P", "Cycle focus to previous window"},
	{"ALT + ^A", "Unfocus all window"},
	{"CTRL + /", "Shutdown the system"}
};

int key_shortcut_count = sizeof(key_shortcut) / sizeof(key_shortcut[0]);

void app_help_render(cosh_win_t *win)
{
	win_attron(win, CP_REG_PURPLE | A_BOLD);
	mvwprintw(win->ptr, 2, 2, "<-- KEY SHORTCUTS -->");
	win_attroff(win, CP_REG_PURPLE | A_BOLD);

	for (int i = 0; i < key_shortcut_count; i++) {
		mvwprintw(win->ptr, i + 4, 2, "%-25s %s", key_shortcut[i].key,
			  key_shortcut[i].description);
	}
}

void win_spawn_help(void)
{
	cosh_win_t *win = win_create(16, 55, WIN_FLAG_NONE);
	if (!win)
		return;

	win_setopt(win, WIN_OPT_APPNAME, "Guide");
	win_setopt(win, WIN_OPT_TITLE, "Guide");
	win_setopt(win, WIN_OPT_RENDER, app_help_render);
}
