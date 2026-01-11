#ifndef KERNEL_H
#define KERNEL_H

#include "log.h"
#include "../wmcurses.h"
#include "../util.h"
#include "../configuration.h"

#include <errno.h>
#include <sys/stat.h>
#include <locale.h>
#include <errno.h>
#include <sys/time.h>
#include <dirent.h>

struct workdir_state {
	long cached_size;
	time_t last_mtime;
	int cached;             /* Flag to avoid pointer access to not existsten memory */
};

int k_get_workdir_usage(void);
int k_boot(void);
void k_shutdown(void);

extern struct workdir_state *wstate;

#endif                          /* KERNEL_H */
