#include "kernel.h"

char WORKDIR[PATH_MAX];
char CONFIGFILE[PATH_MAX];
struct workdir_state *wstate = NULL;

static void generate_default_config(void);

int k_boot(void)
{
	sprintf(WORKDIR, "%s/%s/%s", get_homedir(), XDG_DATA_DEF, WORKDIR_NAME);
	sprintf(CONFIGFILE, "%s/%s", WORKDIR, CONFIGFILENAME);

	struct stat sb;
	if (!setlocale(LC_ALL, ""))
		k_log_warn("Could not set locale from environment.");

	k_log_trace("Ensure workdir...");

	if (lstat(WORKDIR, &sb) != 0) {
		k_log_info("Workdir uninitialized: %s", strerror(errno));
		if (mkdir(WORKDIR, 0755) != 0) {
			k_log_fatal("Failed to create workdir in %s: %s",
				    WORKDIR, strerror(errno));
			return -1;
		}
	} else {
		if (S_ISREG(sb.st_mode)) {
			k_log_error
			    ("Workdir: %s is already exist as regular file (expected: directory)",
			     WORKDIR);
			return -1;
		}

	}
	k_log_info("Workdir is ready %s", WORKDIR);

	k_log_trace("Preparing for workdir state...");
	wstate = malloc(sizeof(*wstate));
	if (!wstate) {
		k_log_fatal("Malloc for workdir state failed.");
		return -1;
	}

	if (lstat(CONFIGFILE, &sb) != 0) {
		k_log_trace("Creating default config...", CONFIGFILE);
		generate_default_config();
	}

	return 0;
}

void k_start(void)
{
	k_log_trace("Loading config...");
	k_load_config();

	k_log_trace("Initializing interface...");
	wm_init();		/* Init TUI */
}

void k_shutdown(void)
{
	int dummy;

	wm_cleanup_before_exit();
	free(wstate);
	cleanup_empty_files(WORKDIR, &dummy);
	endwin();
}

int k_get_workdir_usage(void)
{
	struct stat st;
	if (lstat(WORKDIR, &st) == -1)
		return -1;

	if (wstate->cached == 1 && st.st_mtime == wstate->last_mtime)
		return (int)(wstate->cached_size / 1024);

	if (!S_ISDIR(st.st_mode))
		return st.st_size;

	win_needs_redraw = 1;
	DIR *d = opendir(WORKDIR);
	if (!d)
		return -1;

	long total_size = 0;
	struct dirent *de;
	char path[1024];

	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0
		    || strcmp(de->d_name, "..") == 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s", WORKDIR, de->d_name);

		struct stat fst;
		if (lstat(path, &fst) != -1) {
			total_size += (fst.st_blocks * 512);
		}
	}

	closedir(d);
	wstate->last_mtime = st.st_mtime;
	wstate->cached_size = total_size;
	wstate->cached = true;
	return (int)(total_size / 1024);
}

long k_self_get_rss(void)
{
	static long pagesize = 0;
	long rss = 0;
	FILE *fp = fopen("/proc/self/statm", "r");

	if (pagesize <= 0)
		pagesize = sysconf(_SC_PAGESIZE);

	if (fp) {
		fscanf(fp, "%*s %ld", &rss);
		fclose(fp);
	}

	return rss * pagesize;
}

static const struct {
	const char *key;
	int *target;
	long def_val;
} cfg_colors[] = {
	{"statusbar", &wm.configs.csh_statusbar, 31},
	{"desktop", &wm.configs.csh_desktop, (long)COLOR_BLACK},
};
#define NUM_CFG_COLORS ((int)(sizeof(cfg_colors) / sizeof(cfg_colors[0])))

static void generate_default_config(void)
{
	FILE *fp;

	fp = fopen(CONFIGFILE, "a");
	
	if (!fp) {
		k_log_warn("Failed to open config file: %s", strerror(errno));
		return;
	}

	fprintf(fp, "; https://github.com/John-fried/Cosh.git\n\n");
	fprintf(fp, "[colorscheme]\n");
	for (int i = 0; i < NUM_CFG_COLORS; i++) {
		fprintf(fp, "%s=%d\n", cfg_colors[i].key, (int) cfg_colors[i].def_val);
	}

	fclose(fp);
	k_log_info("Created default config in: %s", CONFIGFILE);
}

int k_load_config(void)
{
	if (access(CONFIGFILE, F_OK) != 0) {
		k_log_warn("Failed to load config in %s: %s", CONFIGFILE, strerror(errno));
		return -1;
	}

	//load colors
	for (int i = 0; i < NUM_CFG_COLORS; i++) {
		*(cfg_colors[i].target) = ini_getl("colorscheme", cfg_colors[i].key, cfg_colors[i].def_val, CONFIGFILE);
	}

	k_log_info("Config loaded.");
	return 0;
}
