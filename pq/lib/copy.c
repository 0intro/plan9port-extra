/*
 * Copy one attribute's value from another, if empty.
 */

#include "all.h"

typedef struct {
	void*	tag;
	int	state;
	char*	derived;
	char*	original;
	Qvec	vec;
	Qref	mark;
	int	last;
} Copy;

enum { Normal, Copyit, Pass1 };

static void
clean(Copy *copy)
{
	int i;

	for (i = 0; copy->vec[i] != nil; i++)
		free(copy->vec[i]);
	copy->vec[0] = nil;
}

void*
copy_open(char **argv)
{
	Copy *copy = malloc(sizeof (Copy));

	copy->tag = pq_open(argv+2);
	if (copy->tag == nil) {
		free(copy);
		return nil;
	}
	copy->derived = strdup(argv[0]);
	copy->original = strdup(argv[1]);
	copy->state = Normal;
	copy->vec[0] = nil;
	return copy;
}

int
copy_close(void *vcopy)
{
	Copy *copy = vcopy;
	int rv;

	rv = pq_close(copy->tag);
	clean(copy);
	free(copy->derived);
	free(copy->original);
	free(copy);
	return rv;
}

int
copy_read(void *vcopy, char **argv)
{
	Copy *copy = vcopy;
	Qvec vec;
	int rv, i;

	switch (copy->state) {
	case Normal:
		rv = pq_read(copy->tag, argv);
		break;
	case Copyit:
		rv = pq_read(copy->tag, vec);
		if (rv > 0) {
			for (i = 0; i < copy->last; i++)
				argv[i] = vec[copy->mark[i] && *vec[i] == 0 ? copy->last : i];
			argv[i] = nil;
		}
		break;
	case Pass1:
		rv = pq_read(copy->tag, argv);
		if (rv == 0) {
			copy->state = Normal;
			rv = pq_write(copy->tag, copy->vec);
			if (rv < 0)
				return rv;
			rv = pq_read(copy->tag, argv);
		}
		break;
	default:
		rv = pq_error(0, "cannot happen");
		break;
	}
	return rv;
}

int
copy_write(void *vcopy, char **argv)
{
	Copy *copy = vcopy;
	Qvec vec;
	Qbuf buf;
	char *val;
	int i;

	copy->state = Normal;
	clean(copy);
	for (i = 0; (vec[i] = argv[i]) != nil; i++) {
		copy->vec[i] = nil;
		copy->mark[i] = eqattr(argv[i], copy->derived);
		if (copy->mark[i]) {
			val = strchr(argv[i], '=');
			if (val != nil) {
				copy->state = Pass1;
				strcon(buf, copy->original, val, nil);
				copy->vec[i] = strdup(buf);
			} else
				copy->state = Copyit;
		}
	}
	copy->last = i;
	switch (copy->state) {
	case Copyit:
		vec[i++] = copy->original;
		vec[i] = nil;
		break;
	case Pass1:
		for (i = 0; i < copy->last; i++)
			if (copy->vec[i] == nil)
				copy->vec[i] = strdup(argv[i]);
		copy->vec[i] = nil;
		break;
	}
	return pq_write(copy->tag, vec);
}
