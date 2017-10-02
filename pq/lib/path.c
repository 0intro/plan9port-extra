/*
 * Path match module
 */

#include "all.h"

typedef struct {
	void*	tag;
	char	*attr;
	Qref	rewrite;
	Qbuf	buf;
} Path;

void*
path_open(char **argv)
{
	Path *path = malloc(sizeof (Path));

	path->tag = pq_open(argv+1);
	if (path->tag == nil) {
		free(path);
		return nil;
	}
	path->attr = strdup(argv[0]);
	return path;
}

int
path_close(void *vpath)
{
	Path *path = vpath;
	int rv = pq_close(path->tag);
	free(path->attr);
	free(path);
	return rv;
}

int
path_read(void *vpath, char **argv)
{
	Path *path = vpath;
	int rv, i, last;

	rv = pq_read(path->tag, argv);
	if (rv > 0) {
		for (i = 0; argv[i] != nil; i++)
			if (path->rewrite[i]) {
				last = strlen(argv[i]) - 1;
				if (last >= 0 && argv[i][last] == '/')
					argv[i][last] = 0;
			}
	}
	return rv;
}

int
path_write(void *vpath, char **argv)
{
	Path *path = vpath;
	Qvec vec;
	char *bp, *val;
	int i, last, exact, star;

	bp = path->buf;
	for (i = 0; (vec[i] = argv[i]) != nil; i++) {
		path->rewrite[i] = eqattr(argv[i], path->attr);
		if (path->rewrite[i]) {
			val = strchr(argv[i], '=');
			if (val == nil)
				continue;
			val++;
			last = strlen(val) - 1;
			exact = (last >= 0 && val[last] == '.');
			star = (last >= 0 && val[last] == '*');
			if (exact || star)
				val[last] = 0;
			vec[i] = bp;
			strncpy(bp, argv[i], val - argv[i]);
			bp += val - argv[i];
			while (*val == '/')
				val++;
			while (*val != 0) {
				while (*val != 0 && *val != '/')
					*bp++ = *val++;
				while (*val == '/')
					val++;
				*bp++ = '/';
			}
			if (star && val[-1] != '/')
				bp--;
			if (!exact)
				*bp++ = '*';
			*bp++ = 0;
		}
	}
	return pq_write(path->tag, vec);
}
