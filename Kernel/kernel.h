#ifndef KERNEL_H
#define KERNEL_H

#include "log.h"
#include "../wmcurses.h"
#include "../util.h"
#include "XDGPATH.h"
#include "configuration.h"

#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <limits.h>
#include <errno.h>
#include <sys/time.h>
#include <dirent.h>

struct workdir_state {
        long cached_size;
        time_t last_mtime;
        int cached;             /* Flag to avoid pointer access to not existsten memory */
};

int k_get_workdir_usage(void);
long k_self_get_rss(void);
int k_boot(void);
void k_shutdown(void);

extern char WORKDIR[PATH_MAX];
extern struct workdir_state *wstate;

#endif                          /* KERNEL_H */
