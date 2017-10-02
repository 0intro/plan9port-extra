/*
 * Regexp-driven attribute name rewriting module
 */

#include "all.h"
#include <regexp.h>

typedef struct {
	void*	tag;
	char*	old;
	int	num;
	Reprog**	re;
	char**	attr;
	Qbuf	buf;
} Rewrite;

void*
rewrite_open(char **argv)
{
	Rewrite *rewrite = malloc(sizeof (Rewrite));
	int n, i;

	for (n = 1; strcmp(argv[n], "--") != 0; n += 2)
		;
	rewrite->tag = pq_open(argv+n+1);
	if (rewrite->tag == nil) {
		free(rewrite);
		return nil;
	}
	rewrite->old = strdup(argv[0]);
	rewrite->num = n/2;
	rewrite->re = malloc(rewrite->num * sizeof(Reprog*));
	rewrite->attr = malloc(rewrite->num * sizeof(char*));
	for (i = 1; i < n; i += 2) {
		strcpy(rewrite->buf, "^");
		strcat(rewrite->buf, argv[i]);
		strcat(rewrite->buf, "$");
		rewrite->re[i/2] = regcomp(rewrite->buf);
		rewrite->attr[i/2] = strdup(argv[i+1]);
	}
	return rewrite;
}

int
rewrite_close(void *vrewrite)
{
	Rewrite *rewrite = vrewrite;
	int rv, i;

	rv = pq_close(rewrite->tag);
	free(rewrite->old);
	for (i = 0; i < rewrite->num; i++) {
		free(rewrite->re[i]);
		free(rewrite->attr[i]);
	}
	free(rewrite);
	return rv;
}

int
rewrite_read(void *vrewrite, char **argv)
{
	Rewrite *rewrite = vrewrite;
	return pq_read(rewrite->tag, argv);
}

int
rewrite_write(void *vrewrite, char **argv)
{
	Rewrite *rewrite = vrewrite;
	Qvec vec;
	char *bp, *val;
	int i, n;

	bp = rewrite->buf;
	for (i = 0; (vec[i] = argv[i]) != nil; i++) {
		if (eqattr(vec[i], rewrite->old)) {
			val = strchr(vec[i], '=');
			if (val == nil)
				continue;
			val++;
			for (n = 0; n < rewrite->num; n++) {
				if (regexec(rewrite->re[n], val, nil, 0)) {
					vec[i] = bp;
					bp = strcon(bp, rewrite->attr[n], "=", val, nil) + 1;
					break;
				}
			}
		}
	}
	return pq_write(rewrite->tag, vec);
}
