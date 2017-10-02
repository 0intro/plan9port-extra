/*
 * Name parsing and matching module
 */

#include "all.h"

#define	REPL	'\001'

typedef struct {
	char*	tag;
	GC	gc, gcw;
	Qvec	attrs, sub, match, pref;
	Qbuf	template;
	char	*blast;
	int	argc;
} Data;

static void
dfree(Data *d)
{
	gcfree(&d->gc);
	gcfree(&d->gcw);
	free(d);
}

void*
name_open(char **argv)
{
	Data *d = calloc(sizeof (Data), 1);
	char *w, *e;
	int i;

	for (i = 0; argv[i] != nil && strcmp(argv[i], "--") != 0; i++)
		d->attrs[i] = gcdup(&d->gc, argv[i]);
	argv += i;
	if (cnt(argv) < 3) {
		pq_error(0, "args: attrs... -- selector pattern");
		dfree(d);
		return nil;
	}
	d->blast = gcdup(&d->gc, argv[1]);

	w = d->template;
	for (e = argv[2]; *e != 0; e++) {
		if (isalpha(*e)) {
			int flag = 0;
			char *s = e;
			char buf[128];

			while (isalnum(*++e));
			strncpy(buf, s, e-s);
			buf[e-s] = 0;
			e--;
			if (e[1] == '=') {
				flag |= 1;
				e++;
			}
			*w++ = REPL;
			*w++ = flag;
			*w++ = uniq(&d->gc, d->sub, buf);
		} else
			*w++ = *e;
	}

	d->tag = pq_open(argv + 3);
	if (d->tag == nil) {
		dfree(d);
		return nil;
	}
print("TEMPLATE:\n");
for (w=d->template; *w; w++) {
	if(*w == REPL) {
		print("«%s%s»", d->sub[w[2]], w[1]? "=":"");
		w += 2;
	}else
		print("%c", *w);
}
print("\n");
	return d;
}

int
name_close(void *vd)
{
	Data *d = vd;
	int rv = pq_close(d->tag);
	dfree(d);
	return rv;
}

int
name_read(void *vd, char **argv)
{
	Data *d = vd;
	Qvec vec;
	char **sub;
	int rv, m, i;

	if (d->match[0] == nil)
		return pq_read(d->tag, argv);

	sub = vec + d->argc;

	while ((rv = pq_read(d->tag, vec)) > 0) {
		for (i = 0; sub[i] != nil; i++) {
			char buf[128], expr[512];
			char **vec[32];
			char *e = expr;

			norm(sub[i], buf, vec);
			*e++ = '(';
			if (*vec[0] != 0) {
				*e++ = *vec[0];
				*e++ = '|';
			}
			e += spread(e, vec, cnt(vec));
			*e++ = '|';
			for (j = 0; *vec[j] != 0; j++)
				for (p = 0; d->pref[p] != nil; p++)
					if (prefix(d->pref[p], vec[j])) {
						e += spread(e, vec, j);
		t = d->template;
		w 
		for (m = 0; d->match[m] != nil; m++)
			if (1) {
				for (i = 0; i < d->argc; i++)
					argv[i] = vec[i];
				argv[i] = nil;
				return rv;
			}
	}
	return rv;
}

int
name_write(void *vd, char **argv)
{
	Data *d = vd;
	Qvec vec;
	int i, a, v;

	gcfree(&d->gcw);
	d->match[0] = d->pref[0] = nil;
	for (i = 0; argv[i] != nil; i++) {
		vec[i] = argv[i];
		for (a = 0; d->attrs[a] != nil; a++)
			if (eqattr(argv[i], d->attrs[a])) {
				char *val = strchr(argv[i], '=');
				if (val != nil) {
					char buf[256], attr[256];
					char *s, *v, *w, *beg;
					int inword = 0;

					w = beg = buf;
					for (s = val+1; *s != 0; s++) {
						if (isalnum(*s) || *s == '*') {
							if (!inword) {
								*w++ = ' ';
								inword++;
								beg = w;
							}
							if (*s == '*') {
								*w = 0;
								uniq(&d->gcw, d->pref, beg);
							}
							*w++ = tolower(*s);
						} else
							inword = 0;
					}
					*w = 0;
// if value is complete empty, then what?
					w = buf + 1;
					v = attr;
					v += sprint(v, "%s=", d->blast);
					s = strrchr(w, ' ');
					if (s != nil) {
						*v++ = *w;
						s++;
					} else {
						*v++ = '.';
						s = w;
					}
					strcpy(v, s);
					vec[i] = gcdup(&d->gcw, attr);
					uniq(&d->gcw, d->match, w);
				}
				break;
			}
	}
	d->argc = i;
	if (d->match[0] != nil) {
		for (v = 0; d->sub[v] != nil; v++)
			vec[i++] = d->sub[v];
	}
	vec[i] = nil;
print("MATCH:\n");
for(i=0; d->match[i]; i++) print("\t%s\n", d->match[i]);
print("PREF:\n");
for(i=0; d->pref[i]; i++) print("\t%s\n", d->pref[i]);

	return pq_write(d->tag, vec);
}

void
norm(char *r, char *w, char **v)
{
	int inword = 0;

	for (; *r != 0; r++) {
		if (isalnum(*r)) {
			if (!inword) {
				*v++ = w;
				inword++;
			}
			*w++ = tolower(*r);
		} else
			inword = 0;
	}
	*w = 0;
	*v = w;
}

static int
uniq(GC *gc, char **a, char *s)
{
	int i;

	for (i = 0; a[i] != nil; i++)
		if (strcmp(a[i], s) == 0)
			return i;
	a[i] = gcdup(gc, s);
	a[i+1] = nil;
	return i;
}

char *
spread(char *e, char **v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		e += sprint(e, "%.*s ?", v[i+1]-v[i], v[i]);
	if (n > 0)
		e -= 2;
	return e;
}
