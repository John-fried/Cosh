#include "config.h"
#include "core.h"

int c_get_workdir_usage(void)
{
	struct stat st;
	if (lstat(WORKDIR, &st) == -1)
		return -1;

	if (wstate->cached == 1 && st.st_mtime == wstate->last_mtime)
		return (int)(wstate->cached_size / 1024);

	if (!S_ISDIR(st.st_mode))
		return st.st_size;

	win_needs_redraw = 1;
	DIR *d = opendir(WORKDIR);
	if (!d)
		return -1;

	long total_size = 0;
	struct dirent *de;
	char path[1024];

	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0
		    || strcmp(de->d_name, "..") == 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s", WORKDIR, de->d_name);

		struct stat fst;
		if (lstat(path, &fst) != -1) {
			total_size += (fst.st_blocks * 512);
		}
	}

	closedir(d);
	wstate->last_mtime = st.st_mtime;
	wstate->cached_size = total_size;
	wstate->cached = true;
	return (int)(total_size / 1024);
}

long c_self_get_rss(void)
{
	static long pagesize = 0;
	long rss = 0;
	FILE *fp = fopen("/proc/self/statm", "r");

	if (pagesize <= 0)
		pagesize = sysconf(_SC_PAGESIZE);

	if (fp) {
		fscanf(fp, "%*s %ld", &rss);
		fclose(fp);
	}

	return rss * pagesize;
}
