
#include "../cosh.h"
#include "../wmcurses.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>

#define JIT_SRC		WORKDIR"/jit_temp.c"
#define JIT_BIN		WORKDIR"/jit_temp.so"
#define JIT_LOG		WORKDIR"/jit_temp.log"


typedef struct {
	char cwd[128];
	char cmd_buf[128];
	char last_out[1024];
} iterm_state_t;

static void load_jit_log(iterm_state_t *sh)
{
	FILE *fp = fopen(JIT_LOG, "r");
	if (fp) {
		size_t n = fread(sh->last_out, 1, sizeof(sh->last_out) - 1, fp);
		sh->last_out[n] = '\0';
		fclose(fp);
	}
}

static void iterm_jit_exec(iterm_state_t *sh)
{
	// Generate C Source
	FILE *fp = fopen(JIT_SRC, "w");
	if (!fp) { strcpy(sh->last_out, "JIT: Failed to create source."); return; }
	
	fprintf(fp, 
		"#include <stdio.h>\n#include <stdlib.h>\n#include <math.h>\n"
		"\nvoid _run() {\n%s\n}\n", sh->cmd_buf);
	fclose(fp);

	// Compile to Shared Object
	char compile_cmd[256];
	snprintf(compile_cmd, sizeof(compile_cmd), 
		 "gcc -O0 -fPIC -shared -o %s %s > %s 2>&1", JIT_BIN, JIT_SRC, JIT_LOG);
	
	if (system(compile_cmd) != 0) {
		load_jit_log(sh); /* show compiler error */
		return;
	}

	// Load & Execute
	void *handle = dlopen(JIT_BIN, RTLD_NOW);
	if (!handle) {
		snprintf(sh->last_out, sizeof(sh->last_out), "DLOPEN Error: %s", dlerror());
		return;
	}

	// Redirect stdout ke log untuk menangkap hasil printf
	int old_stdout = dup(STDOUT_FILENO);
	int log_fd = open(JIT_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	dup2(log_fd, STDOUT_FILENO);

	void (*run_fn)() = dlsym(handle, "_run");
	if (run_fn) run_fn();

	fflush(stdout);
	close(log_fd);
	dup2(old_stdout, STDOUT_FILENO);
	close(old_stdout);

	// Cleanup & Show Result
	dlclose(handle);
	load_jit_log(sh);
	
	unlink(JIT_SRC);
	unlink(JIT_BIN);
}

static void iterm_exec(iterm_state_t *sh)
{
	FILE *fp;
	size_t n;

	if (strlen(sh->cmd_buf) == 0) return;

	/* execute c-jit only if ended with ";" */
	if (sh->cmd_buf[strlen(sh->cmd_buf) - 1] == ';') {
		iterm_jit_exec(sh);
	} 
	else if (strncmp(sh->cmd_buf, "cd ", 3) == 0) {
		if (chdir(sh->cmd_buf + 3) == 0) {
			getcwd(sh->cwd, sizeof(sh->cwd));
			snprintf(sh->last_out, sizeof(sh->last_out), "CWD: %s", sh->cwd);
		} else {
			strcpy(sh->last_out, "cd: error");
		}
	} 
	/* Standard Shell Command */
	else {
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
	mvwprintw(win->ptr, 1, 2, "%s $ %s", sh->cwd, sh->cmd_buf);
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

void win_spawn_iterm(void)
{
	iterm_state_t *sh = calloc(1, sizeof(iterm_state_t));
	getcwd(sh->cwd, sizeof(sh->cwd));
	snprintf(sh->last_out, sizeof(sh->last_out), "Welcome, Super genius.");
	
	cosh_win_t *win = win_create(18, 70, WIN_FLAG_NONE);
	if (!win) { free(sh); return; }

	win_setopt(win, WIN_OPT_TITLE,  "Adams");
	win_setopt(win, WIN_OPT_RENDER, app_iterm_render);
	win_setopt(win, WIN_OPT_INPUT,  app_iterm_input);
	win_setopt(win, WIN_OPT_PRIV,   sh);
}

