#ifndef COSH_H
#define COSH_H

#include "wmcurses.h"
#include "log.h"

#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

#define WORKDIR		".cosh" 

int get_workdir_usage(void);

#endif /* COSH_H */
