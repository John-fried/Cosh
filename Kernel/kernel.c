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

/* Config Management */

const config_item desktop_items[] = {
	{"refresh_rate", "Refresh rate in milliseconds for checking file-descriptor to respond before drawing again", &wm.configs.refresh_rate, "800", TYPE_INT}
};

const config_item color_items[] = {
	{"desktop", "Your cosh desktop background colors", &wm.configs.csh_desktop, "0", TYPE_INT},
	{"statusbar", "", &wm.configs.csh_statusbar, "31", TYPE_INT}
};

const config_section app_config[] = {
	{
		"desktop",
		"Adjust your cosh desktop environment",
		desktop_items,
		sizeof(desktop_items) / sizeof(desktop_items[0])
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
		k_log_warn("Failed to open/create config file: %s", strerror(errno));
		return;
	}

	fprintf(fp, "; Check updates on: https://github.com/John-fried/Cosh.git\n\n");
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

int k_load_config(void) {
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
                    *(int *)it->target = (int)ini_getl(sec->name, it->key, atol(it->def_val), CONFIGFILE);
                    break;

                case TYPE_STR:
                    ini_gets(sec->name, it->key, it->def_val, (char *)it->target, 64, CONFIGFILE);
                    break;

                case TYPE_BOOL:
                    *(bool *)it->target = ini_getbool(sec->name, it->key, (int)atol(it->def_val), CONFIGFILE);
                    break;
            }
        }
    }

    k_log_info("Config loaded..");
    return 0;
}
