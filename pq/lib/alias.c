/*
 * Attribute alias module
 */

#include "all.h"

typedef struct {
	void*	tag;
	char	*old, *new;
	Qbuf	buf;
} Alias;

void*
alias_open(char **argv)
{
	Alias *alias = malloc(sizeof (Alias));

	alias->tag = pq_open(argv+2);
	if (alias->tag == nil) {
		free(alias);
		return nil;
	}
	alias->old = strdup(argv[0]);
	alias->new = strdup(argv[1]);
	return alias;
}

int
alias_close(void *valias)
{
	Alias *alias = valias;
	int rv = pq_close(alias->tag);
	free(alias->old);
	free(alias->new);
	free(alias);
	return rv;
}

int
alias_read(void *valias, char **argv)
{
	Alias *alias = valias;
	return pq_read(alias->tag, argv);
}

int
alias_write(void *valias, char **argv)
{
	Alias *alias = valias;
	Qvec vec;
	char *bp, *val;
	int i;

	bp = alias->buf;
	for (i = 0; (vec[i] = argv[i]) != nil; i++)
		if (eqattr(vec[i], alias->old)) {
			val = strchr(vec[i], '=');
			if (val == nil)
				val = vec[i] + strlen(vec[i]);
			vec[i] = bp;
			bp = strcon(bp, alias->new, val, nil) + 1;
		}
	return pq_write(alias->tag, vec);
}
