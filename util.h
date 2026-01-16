#ifndef UTIL_H
#define UTIL_H

#define CTRL(c)		(((c) ^ ((((c) ^ 0x40) >> 2) & 0x10)) & 0x1f)
#define SET_CHW(var, wide_str) setcchar(&(var), (wide_str), A_NORMAL, 0, NULL)

int levenshtein_distance(const char *s, const char *t);
char *get_homedir(void);
void cleanup_empty_files(const char *dirpath, int *total_deleted);

#endif				/* UTIL_H */
