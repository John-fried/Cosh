#include "core.h"
#include "sys.h"
#include "config.h"

char WORKDIR[PATH_MAX];
char CONFIGFILE[PATH_MAX];
struct workdir_state *wstate = NULL;

int c_boot(void)
{
	snprintf(WORKDIR, sizeof(WORKDIR), "%s/%s/%s", get_homedir(),
		 XDG_DATA_DEF, WORKDIR_NAME);
	snprintf(CONFIGFILE, sizeof(CONFIGFILE), "%s/%s",
		 WORKDIR, CONFIGFILENAME);

	struct stat sb;
	if (!setlocale(LC_ALL, ""))
		c_log_warn("Could not set locale from environment.");

	c_log_trace("Ensure workdir...");

	if (lstat(WORKDIR, &sb) != 0) {
		c_log_info("Workdir uninitialized: %s", strerror(errno));
		if (mkdir(WORKDIR, 0755) != 0) {
			c_log_fatal("Failed to create workdir in %s: %s",
				    WORKDIR, strerror(errno));
			return -1;
		}
	} else {
		if (S_ISREG(sb.st_mode)) {
			c_log_error
			    ("Workdir: %s is already exist as regular file (expected: directory)",
			     WORKDIR);
			return -1;
		}

	}
	c_log_info("Workdir is ready %s", WORKDIR);

	c_log_trace("Preparing for workdir state...");
	wstate = malloc(sizeof(*wstate));
	if (!wstate) {
		c_log_fatal("Malloc for workdir state failed.");
		return -1;
	}

	if (lstat(CONFIGFILE, &sb) != 0) {
		c_log_trace("Creating default config...", CONFIGFILE);
		generate_default_config();
	}

	return 0;
}

void c_start(void)
{
	c_log_trace("Loading config...");
	c_load_config();

	c_log_trace("Initializing interface...");
	wm_init();		/* Init TUI */
}

void c_shutdown(void)
{
	c_log_trace("Shutdowning system...");
	int dummy;

	wm_cleanup_before_exit();
	free(wstate);
	c_log_trace("Cleaning workdir...");
	cleanup_empty_files(WORKDIR, &dummy);
	endwin();
	exit(0);
	_exit(0); //if its still remain
}


