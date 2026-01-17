#ifndef CORE_H
#define CORE_H

#include "log.h"
#include "../wmcurses.h"
#include "../util.h"
#include "XDGPATH.h"
#include "configuration.h"
#include "minIni/minIni.h"	//ini parsing lib

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
	int cached;		/* Flag to avoid pointer access to not existsten memory */
};

int c_get_workdir_usage(void);
long c_self_get_rss(void);
int c_load_config(void);
int c_boot(void);
void c_start(void);
void c_shutdown(void);

extern char WORKDIR[PATH_MAX];
extern char CONFIGFILE[PATH_MAX];

extern struct workdir_state *wstate;

#endif				/* CORE_H */
