#ifndef COSH_H
#define COSH_H

#include "wmcurses.h"
#include "Kernel/kernel.h"
#include "util.h"

#include <string.h>
#include <errno.h>
#include <poll.h>
#include "configuration.h"

typedef struct app_entry {
	char *name;
	void (*spawn)(void);
	int score;		/* for levenshtein */
	struct app_entry *next;	/* for linked list */
} app_entry_t;

extern app_entry_t *app_registry;
extern int app_count;

void register_app(const char *name, void (*spawn)(void));

#endif				/* COSH_H */
