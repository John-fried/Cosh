#include "../wmcurses.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

typedef struct {
	char cwd[128];
	char cmd_buf[128];
	char last_out[1024];
} iterm_state_t;

static void iterm_exec(iterm_state_t *sh)
{
	FILE *fp;
	size_t n;

	if (strlen(sh->cmd_buf) == 0)
		return;

	if (strncmp(sh->cmd_buf, "cd ", 3) == 0) {
		if (chdir(sh->cmd_buf + 3) == 0) {
			getcwd(sh->cwd, sizeof(sh->cwd));
			snprintf(sh->last_out, sizeof(sh->last_out), "Directory: %s", sh->cwd);
		} else {
			strcpy(sh->last_out, "cd: error");
		}
	} else {
		fp = popen(sh->cmd_buf, "r");
		if (fp) {
			n = fread(sh->last_out, 1, sizeof(sh->last_out) - 1, fp);
			sh->last_out[n] = '\0';
			pclose(fp);
		} else {
			strcpy(sh->last_out, "Exec failed.");
		}
	}
	sh->cmd_buf[0] = '\0';
}

void app_iterm_input(cosh_win_t *win, int ch)
{
	iterm_state_t *sh = (iterm_state_t *)win->priv;
	int len = strlen(sh->cmd_buf);

	if (ch == '\n' || ch == KEY_ENTER) {
		iterm_exec(sh);
	} else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
		if (len > 0) sh->cmd_buf[len - 1] = '\0';
	} else if (ch >= 32 && ch <= 126 && len < 120) {
		sh->cmd_buf[len] = (char)ch;
		sh->cmd_buf[len + 1] = '\0';
	}
}

void app_iterm_render(cosh_win_t *win)
{
	iterm_state_t *sh = (iterm_state_t *)win->priv;
	char *token, *saveptr;
	char *out_copy;
	int row = 3;

	wattron(win->ptr, A_BOLD);
	mvwprintw(win->ptr, 1, 2, "God@Cosh:%s$ %s", sh->cwd, sh->cmd_buf);
	wattroff(win->ptr, A_BOLD);

	mvwhline(win->ptr, 2, 1, ACS_HLINE, win->w - 2);

	out_copy = strdup(sh->last_out);
	token = strtok_r(out_copy, "\n", &saveptr);
	while (token && row < win->h - 1) {
		mvwprintw(win->ptr, row++, 2, "%.*s", win->w - 4, token);
		token = strtok_r(NULL, "\n", &saveptr);
	}
	free(out_copy);
}

/* Helper to initialize the iterm window */
void win_spawn_iterm(void)
{
	cosh_win_t *win = win_create(18, 70, "System Terminal", 
				     app_iterm_render, app_iterm_input);
	if (!win) return;

	iterm_state_t *sh = calloc(1, sizeof(iterm_state_t));
	getcwd(sh->cwd, sizeof(sh->cwd));
	
	/* v7 Welcome Message as initial state */
	snprintf(sh->last_out, sizeof(sh->last_out), 
		 "Welcome, Super Genius.\n"
		 "Let's obey God together to use this OS.\n"
		 "Type commands below.");
	
	win->priv = sh;
}
