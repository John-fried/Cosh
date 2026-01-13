#include "kernel.h"

char WORKDIR[PATH_MAX];
struct workdir_state *wstate = NULL;

int k_boot(void)
{
	sprintf(WORKDIR, "%s/%s/%s", get_homedir(), XDG_DATA_DEF, WORKDIR_NAME);

        struct stat sb;
        setlocale(LC_ALL, "");

        k_log_trace("Ensure workdir...");

        if (lstat(WORKDIR, &sb) == -1) {
                k_log_info("Workdir uninitialized: %s", strerror(errno));
                if (mkdir(WORKDIR, 0755) != 0) {
			k_log_fatal("Failed to create workdir in %s: %s", WORKDIR, strerror(errno));
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
        k_log_info("Workdir ready at: %s", WORKDIR);

        k_log_trace("Preparing for workdir state...");
        wstate = malloc(sizeof(*wstate));
        if (!wstate) {
                k_log_fatal("Malloc for workdir state failed.");
                return -1;
        }
        k_log_info("Workdir state ready");

        k_log_trace("Initializing Graphic...");
        wm_init();              /* Init TUI */

        return 0;
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
