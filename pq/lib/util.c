#include "all.h"

char	pq_errmsg[512];

int
pq_error(int eflag, char *fmt, ...)
{
	char *out, *end;
	va_list arg;

	out = pq_errmsg;
	end = pq_errmsg + sizeof pq_errmsg;

	va_start(arg, fmt);
	out = vseprint(out, end, fmt, arg);
	va_end(arg);
	if (eflag)
		vseprint(out, end, ": %r", 0);
	return -1;
}

char *
pq_path(char *file)
{
	static char root[256];

	if (strcmp(file, ".") == 0
	|| strncmp(file, "/", 1) == 0
	|| strncmp(file, "./", 2) == 0
	|| strncmp(file, "../", 3) == 0)
		return file;
	sprint(root, "/lib/pq/%s", file);
	return root;
}

int
eqattr(char *a1, char *a2)
{
	while (*a1 && *a1 == *a2)
		a1++, a2++;
	return (!*a1 && *a2 == '=') || (!*a1 && !*a2)
	    || (!*a2 && *a1 == '=');
}

char *
strcon(char *dest, ...)
{
	va_list ap;
	char *s;

	va_start(ap, dest);
	while ((s = va_arg(ap, char*)) != nil) {
		while ((*dest++ = *s++) != 0)
			;
		dest--;
	}
	va_end(ap);
	return dest;
}

int
strvec(char *str, char **vec, char *sep)
{
	int i;

	for (i = 0; (vec[i] = strtok(i != 0 ? nil : str, sep)) != nil; i++)
		;
	return i;
}

int
strvecc(char *str, char **vec, char sepc)
{
	int i;

	for (i = 0; (vec[i] = str) != nil; i++)
		if ((str = strchr(str, sepc)) != nil)
			*str++ = 0;
	return i;
}

#ifndef PLAN9PORT
char*
strndup(char *s, int n)
{
	return strncpy(malloc(n+1), s, n+1);
}
#endif

int
cnt(char **argv)
{
	int n = 0;
	while (argv[n++] != nil);
	return n;
}

void*
gcalloc(GC *gc, int n)
{
	void *v;

	v = calloc(n, 1);
	if (v == nil)
		abort();
	if (gc != nil) {
		if (gc->cur == gc->max) {
			gc->max += 100;
			gc->ptrs = realloc(gc->ptrs, gc->max * sizeof (void*));
			if (gc->ptrs == nil)
				abort();
		}
		gc->ptrs[gc->cur++] = v;
	}
	return v;
}

void
gcfree(GC *gc)
{
	int i;

	for (i = 0; i < gc->cur; i++)
		free(gc->ptrs[i]);
	if (gc->ptrs != nil)
		free(gc->ptrs);
	gc->cur = gc->max = 0;
	gc->ptrs = nil;
}

char*
gcdup(GC *gc, char *s)
{
	return strcpy(gcalloc(gc, strlen(s)+1), s);
}
