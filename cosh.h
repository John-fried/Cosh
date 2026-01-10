#ifndef COSH_H
#define COSH_H

#include "wmcurses.h"
#include "kernel/log.h"
#include "util.h"

#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <locale.h>

#include "configuration.h"

struct workdir_state {
        long cached_size;
        time_t last_mtime;
        int cached;             /* Flag to avoid pointer access to not existsten memory */
};

int get_workdir_usage(void);

typedef struct app_entry {
        char *name;
        void (*spawn)(void);
        int score;              /* for levenshtein */
        struct app_entry *next; /* for linked list */
} app_entry_t;

extern app_entry_t *app_registry;
extern int app_count;
extern struct workdir_state *wstate;

void register_app(const char *name, void (*spawn)(void));

#endif                          /* COSH_H */
