#include "../util.h"
#include <string.h>
#include <stdlib.h>

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

int levenshtein_distance(const char *s, const char *t)
{
	int ls = strlen(s);
	int lt = strlen(t);
	int *d = malloc((ls + 1) * (lt + 1) * sizeof(int));
	int res;

	for (int i = 0; i <= ls; i++)
		d[i * (lt + 1)] = i;
	for (int j = 0; j <= lt; j++)
		d[j] = j;

	for (int j = 1; j <= lt; j++) {
		for (int i = 1; i <= ls; i++) {
			if (s[i - 1] == t[j - 1])
				d[i * (lt + 1) + j] =
				    d[(i - 1) * (lt + 1) + (j - 1)];
			else
				d[i * (lt + 1) + j] =
				    MIN3(d[(i - 1) * (lt + 1) + j] + 1,
					 d[i * (lt + 1) + (j - 1)] + 1,
					 d[(i - 1) * (lt + 1) + (j - 1)] + 1);
		}
	}

	res = d[ls * (lt + 1) + lt];
	free(d);
	return res;
}
