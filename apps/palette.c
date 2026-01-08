#include "../wmcurses.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

void win_spawn_iterm(void);
void win_spawn_help(void);

typedef struct {
	char *name;
	void (*spawn)(void);
	int score;
} app_entry_t;

static app_entry_t registry[] = {
	{ "System Terminal", win_spawn_iterm, 0 },
	{ "Quick Help",       win_spawn_help,  0 },
	{ "Manual Browser",   NULL,            0 },
	{ "Task Manager",     NULL,            0 },
	{ "God's Editor",     NULL,            0 }
};

#define REG_SIZE (sizeof(registry) / sizeof(app_entry_t))

typedef struct {
	char query[64];
	int selection;
} palette_state_t;

static void update_scores(palette_state_t *st)
{
	for (size_t i = 0; i < REG_SIZE; i++) {
		if (strlen(st->query) == 0)
			registry[i].score = i;
		else
			registry[i].score = levenshtein_distance(st->query, registry[i].name);
	}

	for (size_t i = 0; i < REG_SIZE - 1; i++) {
		for (size_t j = 0; j < REG_SIZE - i - 1; j++) {
			if (registry[j].score > registry[j + 1].score) {
				app_entry_t tmp = registry[j];
				registry[j] = registry[j + 1];
				registry[j + 1] = tmp;
			}
		}
	}
}

void app_palette_input(cosh_win_t *win, int ch)
{
	palette_state_t *st = (palette_state_t *)win->priv;
	int len = strlen(st->query);

	if (ch == KEY_MOUSE) { win->dirty = 1; return; }

	if (ch == '\n' || ch == KEY_ENTER) {
		void (*spawn_ptr)(void) = registry[st->selection].spawn;
		win_destroy_focused();
		if (spawn_ptr) spawn_ptr();
		return;
	} else if (ch == KEY_UP) {
		if (st->selection > 0) st->selection--;
	} else if (ch == KEY_DOWN) {
		if (st->selection < (int)REG_SIZE - 1) st->selection++;
	} else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
		if (len > 0) st->query[len - 1] = '\0';
		update_scores(st);
	} else if (ch >= 32 && ch <= 126 && len < 60) {
		st->query[len] = (char)ch;
		st->query[len + 1] = '\0';
		update_scores(st);
	}
}

void app_palette_render(cosh_win_t *win)
{
	palette_state_t *st = (palette_state_t *)win->priv;

	mvwprintw(win->ptr, 1, 2, "Search: [ %-32s ]", st->query);
	mvwhline(win->ptr, 2, 1, ACS_HLINE, win->w - 2);

	for (size_t i = 0; i < REG_SIZE; i++) {
		if (i == (size_t)st->selection)
			wattron(win->ptr, A_REVERSE);
		mvwprintw(win->ptr, 3 + i, 2, " %-20s (dist: %d)", registry[i].name, registry[i].score);
		wattroff(win->ptr, A_REVERSE);
	}
}

void win_spawn_palette(void)
{
	palette_state_t *st = calloc(1, sizeof(palette_state_t));
	update_scores(st);

	cosh_win_t *win = win_create(15, 50, WIN_FLAG_NONE);
	if (!win) { free(st); return; }

	win_setopt(win, WIN_OPT_TITLE,  "Apps Palette");
	win_setopt(win, WIN_OPT_RENDER, app_palette_render);
	win_setopt(win, WIN_OPT_INPUT,  app_palette_input);
	win_setopt(win, WIN_OPT_PRIV,   st);
}
