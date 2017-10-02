/*
 * Query optimization module
 */

#include "all.h"

typedef struct {
	void*	tag;
	int	count;
	Qref	copy;
} Opt;

void*
opt_open(char **argv)
{
	Opt *opt = malloc(sizeof (Opt));

	opt->tag = pq_open(argv);
	if (opt->tag == nil) {
		free(opt);
		return 0;
	}
	opt->count = -1;
	return opt;
}

int
opt_close(void *vopt)
{
	Opt *opt = vopt;
	int rv = pq_close(opt->tag);
	free(opt);
	return rv;
}

int
opt_read(void *vopt, char **argv)
{
	Opt *opt = vopt;
	int rv, i;

	rv = pq_read(opt->tag, argv);
	if (rv > 0)
		for (i = opt->count; i >= 0; i--)
			argv[i] = argv[opt->copy[i]];
	return rv;
}

int
opt_write(void *vopt, char **argv)
{
	Opt *opt = vopt;
	int i, j, k = 0;
	Qvec vec;

	for (i = 0; argv[i]; i++) {
		for (j = 0; j < k; j++)
			if (eqattr(argv[i], vec[j])) {
				if (strchr(vec[j], '=') == nil)
					vec[j] = argv[i];
				break;
			}
		if (j == k)
			vec[k++] = argv[i];
		opt->copy[i] = j;
	}
	opt->copy[i] = k;
	opt->count = i;
	vec[k] = nil;
	return pq_write(opt->tag, vec);
}
