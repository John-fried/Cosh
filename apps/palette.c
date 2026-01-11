#include "../wmcurses.h"
#include "../util.h"
#include "../cosh.h"

#include <stdlib.h>
#include <string.h>

/**
 * palette_state_t - Encapsulated state for the apps palette
 * * This structure follows OOP principles by grouping all relevant
 * data for the palette component.
 */
typedef struct {
	char query[64];
	int selection;
} palette_state_t;

/**
 * palette_sort - Bubble sort the app registry by Levenshtein score
 * @note: Since app_registry is small, bubble sort is acceptable,
 * but the logic is optimized to stop early if no swaps occur.
 */
static void palette_sort(void)
{
	int swapped;
	app_entry_t *ptr1;
	app_entry_t *lptr = NULL;

	if (!app_registry || !app_registry->next)
		return;

	do {
		swapped = 0;
		ptr1 = app_registry;

		while (ptr1->next != lptr) {
			/* Lower score means higher relevance */
			if (ptr1->score > ptr1->next->score) {
				char *temp_name = ptr1->name;
				void (*temp_spawn)(void) = ptr1->spawn;
				int temp_score = ptr1->score;

				ptr1->name = ptr1->next->name;
				ptr1->spawn = ptr1->next->spawn;
				ptr1->score = ptr1->next->score;

				ptr1->next->name = temp_name;
				ptr1->next->spawn = temp_spawn;
				ptr1->next->score = temp_score;

				swapped = 1;
			}
			ptr1 = ptr1->next;
		}
		lptr = ptr1;
	} while (swapped);
}

/**
 * palette_update_relevance - Calculate scores for all apps based on query
 */
static void palette_update_relevance(palette_state_t *st)
{
	app_entry_t *curr = app_registry;
	int qlen = strlen(st->query);

	while (curr) {
		if (qlen == 0)
			curr->score = 0;
		else
			curr->score =
			    levenshtein_distance(st->query, curr->name);
		curr = curr->next;
	}

	palette_sort();
}

/**
 * palette_get_app_by_index - Retrieve app entry from linked list by position
 */
static app_entry_t *palette_get_app_by_index(int index)
{
	app_entry_t *curr = app_registry;
	int i = 0;

	while (curr && i < index) {
		curr = curr->next;
		i++;
	}
	return curr;
}

/**
 * app_palette_render - Render the palette UI
 */
void app_palette_render(cosh_win_t *win)
{
	palette_state_t *st = (palette_state_t *) win->priv;
	app_entry_t *curr = app_registry;
	int i = 0;

	/* Draw Search Box */
	mvwprintw(win->ptr, 1, 2, "Search: [ %-32s ]", st->query);
	mvwhline(win->ptr, 2, 1, ACS_HLINE, win->w - 2);

	/* Traverse linked list for rendering */
	while (curr && (3 + i) < win->h - 1) {
		if (i == st->selection)
			wattron(win->ptr, A_REVERSE);

		mvwprintw(win->ptr, 3 + i, 2, " %-20s", curr->name);

		wattroff(win->ptr, A_REVERSE);
		curr = curr->next;
		i++;
	}

	if (app_count == 0)
		mvwprintw(win->ptr, 3, 2, " No apps registered.");
}

/**
 * app_palette_input - Handle user interaction
 */
void app_palette_input(cosh_win_t *win, int ch)
{
	palette_state_t *st = (palette_state_t *) win->priv;
	int qlen = strlen(st->query);

	if (ch == KEY_MOUSE) {
		win->dirty = 1;
		return;
	}

	switch (ch) {
	case '\n':
	case KEY_ENTER:{
			app_entry_t *selected =
			    palette_get_app_by_index(st->selection);
			if (selected && selected->spawn) {
				void (*spawn_fn)(void) = selected->spawn;
				win_destroy_focused();
				spawn_fn();
			}
			break;
		}
	case KEY_UP:
		if (st->selection > 0)
			st->selection--;
		break;
	case KEY_DOWN:
		if (st->selection < app_count - 1)
			st->selection++;
		break;
	case KEY_BACKSPACE:
	case 127:
	case 8:
		if (qlen > 0) {
			st->query[qlen - 1] = '\0';
			palette_update_relevance(st);
			st->selection = 0;
		}
		break;
	default:
		if (ch >= 32 && ch <= 126 && qlen < 60) {
			st->query[qlen] = (char)ch;
			st->query[qlen + 1] = '\0';
			palette_update_relevance(st);
			st->selection = 0;
		}
		break;
	}
}

/**
 * win_spawn_palette - Entry point to create the palette window
 */
void win_spawn_palette(void)
{
	palette_state_t *st = calloc(1, sizeof(palette_state_t));
	if (!st)
		return;

	palette_update_relevance(st);

	/* Fixed size palette for consistent UI */
	cosh_win_t *win = win_create(15, 50, WIN_FLAG_NONE);
	if (!win) {
		free(st);
		return;
	}

	win_setopt(win, WIN_OPT_TITLE, "Apps Palette");
	win_setopt(win, WIN_OPT_RENDER, app_palette_render);
	win_setopt(win, WIN_OPT_INPUT, app_palette_input);
	win_setopt(win, WIN_OPT_PRIV, st);
}
