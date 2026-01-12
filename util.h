#ifndef UTIL_H
#define UTIL_H

#define SET_CHW(var, wide_str) (var) = (cchar_t){0, {wide_str}, 0}

int levenshtein_distance(const char *s, const char *t);
void cleanup_empty_files(const char *dirpath, int *total_deleted);

#endif                          /* UTIL_H */
