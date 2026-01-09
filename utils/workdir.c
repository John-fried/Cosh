#include "../util.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

void cleanup_empty_files(const char *dirpath, int *total_deleted)
{
        DIR *d;
        struct dirent *entry;
        struct stat statbuf;
        char full_path[PATH_MAX];

        d = opendir(dirpath);

        if (!d) {
                fprintf(stderr, "failed to open directory %s: %s\n",
                                dirpath, strerror(errno));
                return;
        }

        while ((entry = readdir(d)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 ||
                        strcmp(entry->d_name, "..") == 0)
                        continue;

                snprintf(full_path, sizeof(full_path), "%s/%s",
                                dirpath, entry->d_name);

                if (lstat(full_path, &statbuf) == -1) {
                        perror("lstat_error");
                        continue;
                }

                if (S_ISDIR(statbuf.st_mode))
                        cleanup_empty_files(full_path, total_deleted);
                else if (S_ISREG(statbuf.st_mode) && statbuf.st_size == 0) {
                        if (unlink(full_path) == 0) {
                                printf("deleted: %s/%s\n", dirpath, entry->d_name);
                                (*total_deleted)++;
                        } else {
                                printf("failed to delete file: %s (%s)\n", entry->d_name,
                                                strerror(errno));
                        }
                }
        }

        closedir(d);
}
