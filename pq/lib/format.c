/*
 * Formatted virtual attributes module
 */

#include "all.h"

typedef struct {
	char*	tag;
	char	attr[64];
	char*	attrs[64];
	Qbuf	expr, prog;
	Qref	insert;
	int	argc, tail;
} Format;

static char usage[] = "format: usage: attr fmtexpr [modules]";

void*
format_open(char **argv)
{
	Format *format = malloc(sizeof (Format));

	if (cnt(argv) < 2) {
		pq_error(0, usage);
		free(format);
		return nil;
	}
	strcpy(format->attr, argv[0]);
	strcon(format->expr, argv[1], "\\c", nil);
	fmtcomp(format->prog, format->expr, format->attrs);
	format->tag = pq_open(argv + 2);
	if (format->tag == nil) {
		free(format);
		return nil;
	}
	return format;
}

int
format_close(void *vformat)
{
	Format *format = vformat;
	int rv = pq_close(format->tag);
	free(format);
	return rv;
}

int
format_read(void *vformat, char **argv)
{
	Format *format = vformat;
	Qvec vec;
	Qbuf val;
	int rv, v, i;

	if (format->tail == format->argc)
		return pq_read(format->tag, argv);

	rv = pq_read(format->tag, vec);
	if (rv > 0) {
		fmtexec(val, format->prog, vec + format->tail);
		v = 0;
		for (i = 0; i < format->argc; i++)
			argv[i] = format->insert[i] ? val : vec[v++];
		argv[i] = nil;
	}
	return rv;
}

int
format_write(void *vformat, char **argv)
{
	Format *format = vformat;
	Qvec vec;
	int v, i;

	v = 0;
	for (i = 0; argv[i] != nil; i++) {
		format->insert[i] = eqattr(argv[i], format->attr);
		if (!format->insert[i])
			vec[v++] = argv[i];
	}
	format->argc = i;
	format->tail = v;
	if (v < i)
		for (i = 0; format->attrs[i] != nil; i++)
			vec[v++] = format->attrs[i];
	vec[v] = nil;
	return pq_write(format->tag, vec);
}
