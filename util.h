#ifndef UTIL_H
#define UTIL_H

int levenshtein_distance(const char *s, const char *t);
void cleanup_empty_files(const char *dirpath, int *total_deleted);

#endif                          /* UTIL_H */
