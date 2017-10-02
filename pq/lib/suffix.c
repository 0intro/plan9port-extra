/*
 * Suffix match translation module
 */

#include "all.h"

typedef struct {
	char*	tag;
	char*	attr[64];
	Qref	reverse;
	Qbuf	buf;
} Suffix;

static void clean(Suffix*);
static char* reverse(char*);

void*
suffix_open(char **argv)
{
	Suffix *suffix = malloc(sizeof (Suffix));
	int i;

	for (i = 0; argv[i] != nil && strcmp(argv[i], "--") != 0; i++)
		suffix->attr[i] = strdup(argv[i]);
	suffix->attr[i] = nil;
	if (argv[i] != nil)
		i++;
	suffix->tag = pq_open(argv + i);
	if (suffix->tag == nil) {
		clean(suffix);
		return nil;
	}
	return suffix;
}

int
suffix_close(void *vsuffix)
{
	Suffix *suffix = vsuffix;
	int rv = pq_close(suffix->tag);
	clean(suffix);
	return rv;
}

int
suffix_read(void *vsuffix, char **argv)
{
	Suffix *suffix = vsuffix;
	char *cp = suffix->buf;
	char *val;
	int rv, i;

	rv = pq_read(suffix->tag, argv);
	if (rv > 0) {
		for (i = 0; argv[i] != nil; i++)
			if (suffix->reverse[i]) {
				val = argv[i];
				argv[i] = cp;
				cp = strcon(cp, reverse(val), nil) + 1;
			}
	}
	return rv;
}

int
suffix_write(void *vsuffix, char **argv)
{
	Suffix *suffix = vsuffix;
	char *cp = suffix->buf;
	Qvec vec;
	char **ap;
	char *val;
	int len, i;

	for (i = 0; argv[i] != nil; i++) {
		suffix->reverse[i] = 0;
		vec[i] = argv[i];
		val = strchr(argv[i], '=');
		len = strlen(argv[i]);
		if (val == nil || argv[i][len-1] == '*')
			continue;
		for (ap = suffix->attr; *ap != nil; ap++)
			if (eqattr(argv[i], *ap)) {
				suffix->reverse[i] = 1;
				vec[i] = cp;
				cp = strcon(cp, *ap, "r=", reverse(val+1), nil) + 1;
				break;
			}
	}
	vec[i] = nil;
	return pq_write(suffix->tag, vec);
}

static void
clean(Suffix *suffix)
{
	char **ap;

	for (ap = suffix->attr; *ap != nil; ap++)
		free(*ap);
	free(suffix);
}

static char*
reverse(char *s)
{
	static Qbuf buf;
	int i;

	i = strlen(s);
	buf[i] = 0;
	while (*s != 0)
		buf[--i] = *s++;
	return buf;
}
