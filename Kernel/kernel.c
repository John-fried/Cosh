#include "kernel.h"

char WORKDIR[PATH_MAX];
char CONFIGFILE[PATH_MAX];
struct workdir_state *wstate = NULL;

static void generate_default_config(void);

int k_boot(void)
{
	snprintf(WORKDIR, sizeof(WORKDIR), "%s/%s/%s", get_homedir(),
		 XDG_DATA_DEF, WORKDIR_NAME);
	snprintf(CONFIGFILE, strlen(WORKDIR) + strlen(CONFIGFILENAME), "%s/%s",
		 WORKDIR, CONFIGFILENAME);

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
	k_log_trace("Shutdowning system...");
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

/* Config Management */

#define CFG_INT(k, d, t, v) { k, d, t, v, TYPE_INT, 0 }
#define CFG_STR(k, d, t, v) { k, d, t, v, TYPE_STR, sizeof(t) }
#define CFG_BOOL(k, d, t, v) { k, d, t, v, TYPE_BOOL, 0 }

static const config_item desktop_items[] = {
	CFG_INT("refresh_rate", "Refresh rate in milliseconds",
		&wm.configs.desktop.refresh_rate, "800")
};

static const config_item key_items[] = {
	CFG_INT("modifier", "27 is Alt", &wm.configs.keys.modifier, "27"),
	CFG_STR("win_mv_up", "Move focused window up",
		wm.configs.keys.win_mv_up, "k"),
	CFG_STR("win_mv_down", "Move focused window down",
		wm.configs.keys.win_mv_down, "j"),
	CFG_STR("win_mv_right", "Move focused window right",
		wm.configs.keys.win_mv_right, "l"),
	CFG_STR("win_mv_left", "Move focused window left",
		wm.configs.keys.win_mv_left, "h"),
};

static const config_item color_items[] = {
	CFG_INT("desktop", "Desktop background colors",
		&wm.configs.colorscheme.desktop, "232"),
	CFG_INT("standard", "Standard window foreground",
		&wm.configs.colorscheme.standard, "7"),
	CFG_INT("standard_bg", "Standard window background",
		&wm.configs.colorscheme.standard_bg, "0"),
	CFG_INT("cursor", "Cursor color", &wm.configs.colorscheme.cursor, "0"),
	CFG_INT("cursor_bg", "Cursor background",
		&wm.configs.colorscheme.cursor_bg, "8"),
	CFG_INT("accent", "Accent color", &wm.configs.colorscheme.accent, "1"),
	CFG_INT("statusbar", "Statusbar text",
		&wm.configs.colorscheme.statusbar, "7"),
	CFG_INT("statusbar_bg", "Statusbar background",
		&wm.configs.colorscheme.statusbar_bg, "31")
};

static const config_section app_config[] = {
	{
	 "desktop",
	 "Adjust your cosh desktop environment",
	 desktop_items,
	 sizeof(desktop_items) / sizeof(desktop_items[0])
	 },
	{
	 "keys",
	 "Adjust your key shortcut here",
	 key_items,
	 sizeof(key_items) / sizeof(key_items[0])
	 },
	{
	 "colorscheme",
	 "Adjust your cosh colorscheme/theme. Based on the ncurses color id.",
	 color_items,
	 sizeof(color_items) / sizeof(color_items[0])
	 }
};

#define NUM_SECTIONS ((int)(sizeof(app_config) / sizeof(app_config[0])))

static void generate_default_config(void)
{
	FILE *fp;
	fp = fopen(CONFIGFILE, "w");

	if (!fp) {
		k_log_warn("Failed to open/create config file: %s",
			   strerror(errno));
		return;
	}

	fprintf(fp,
		"; Check updates on: https://github.com/John-fried/Cosh.git\n\n");
	for (int i = 0; i < NUM_SECTIONS; i++) {
		const config_section *sec = &app_config[i];
		fprintf(fp, "[%s]\n", sec->name);

		//section desc
		if (strlen(sec->desc) > 0)
			fprintf(fp, "; %s\n", sec->desc);

		for (int j = 0; j < sec->item_count; j++) {
			const config_item *it = &sec->items[j];
			fprintf(fp, "%s=%s", it->key, it->def_val);

			if (strlen(it->desc) > 0)
				fprintf(fp, "\t; %s", it->desc);

			fprintf(fp, "\n");
		}

		fprintf(fp, "\n");
	}

	fclose(fp);
	k_log_info("Created default config in: %s", CONFIGFILE);
}

int k_load_config(void)
{
	if (access(CONFIGFILE, F_OK) != 0) {
		k_log_warn("Config file not found: %s", CONFIGFILE);
		return -1;
	}

	for (int i = 0; i < NUM_SECTIONS; i++) {
		const config_section *sec = &app_config[i];

		for (int j = 0; j < sec->item_count; j++) {
			const config_item *it = &sec->items[j];

			switch (it->type) {
			case TYPE_INT:
				*(int *)it->target =
				    (int)ini_getl(sec->name, it->key,
						  atol(it->def_val),
						  CONFIGFILE);
				break;

			case TYPE_STR:
				ini_gets(sec->name, it->key, it->def_val,
					 (char *)it->target, (int)it->len,
					 CONFIGFILE);
				break;

			case TYPE_BOOL:
				*(bool *)it->target =
				    ini_getbool(sec->name, it->key,
						(int)atol(it->def_val),
						CONFIGFILE);
				break;
			}
		}
	}

	k_log_info("Config loaded..");
	return 0;
}
