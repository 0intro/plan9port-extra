/*
 * Join relations module
 */

#include "all.h"

#define	VERSION	"plan 9 pq version 4423"

#define	NBits	512		/* # of bits in a Vec vector */
#define	BpB	8		/* # of bits per byte */

#define	RELNO(rp)	((rp) - join->rel)

typedef char Vec[NBits/BpB];
typedef enum {
	Vtst, Vtand, Vtnand, Vclr, Vcpy, Vor, Vand, Vnand
} Vcmd;

typedef struct {
	char*	attr;
	char*	val;
	int	a;
} Query;

typedef struct {
	int	present, mark;
	Vec	attrs, keys, deps;
	char*	pq;
	char**	mods[64];
} Rel;

typedef struct {
	Vec	attrs;
	int	wrote;
	char*	base;
	Rel*	rp;
} Merge;

typedef struct {
	int	list;
	int	version;
	Rel*	rp;
	Rel*	defrel;
	Query	query[PQ_VEC];
	Merge	merge[NBits];
	Rel	rel[NBits];
	char*	attr[NBits];
	Vec	lowpri;
	char	base[PQ_REC];
	char	dispatch[8192];
	char*	disptr;
	char*	vectors[2048];
	char**	vecptr;
} Join;

static char**	nextline(Join*);
static int		relopen(Join*, Rel*);
static int		pqowrite(Join*, Rel*, char**);
static int		Vop(Vcmd, Vec, Vec);
static int		Vop1(Vcmd, Vec, int);
static int		Vsize(Vec);
static void		dump(Join*, Rel*, char*, Vec);
static int		debug = 0;

void*
join_open(char **argv)
{
	Join *join = malloc(sizeof (Join));
	int fd, n, na, m, i, j;
	Vec globals;
	char **vec;
	char *arg;
	Rel *rp;

	while (argv[0] != nil && argv[0][0] == '-') {
		if (argv[0][1] == 'd') {
			if (argv[0][2] == 0) {
				if (argv[1] == nil) {
					pq_error(0, "join: -d takes a numeric argument");
					free(join);
					return nil;
				}
				arg = argv[1];
				argv++;
			} else
				arg = argv[0] + 2;
			n = atoi(arg);
			if (n <= 0) {
				pq_error(0, "join: %s: -d takes a numeric argument", arg);
				free(join);
				return nil;
			}
			debug += n;
			argv++;
		} else {
			pq_error(0, "join: %s: unknown option", argv[0]);
			free(join);
			return nil;
		}
	}

	if (argv[0] == nil)
		argv[0] = "dispatch";
	arg = pq_path(argv[0]);
	fd = open(arg, OREAD);
	if (fd == -1) {
		pq_error(-1, "join read %s", arg);
		free(join);
		return nil;
	}
	n = read(fd, join->dispatch, sizeof join->dispatch);
	if (n <= 0 || n == sizeof join->dispatch) {
		pq_error(0, "join %s: file too %s", arg, n <= 0 ? "small" : "big");
		free(join);
		return nil;
	}
	join->dispatch[n] = 0;
	join->disptr = join->dispatch;
	join->vecptr = join->vectors;
	close(fd);

	join->rp = nil;
	join->defrel = nil;
	join->list = -1;
	join->version = -1;
	Vop(Vclr, join->lowpri, 0);
	Vop(Vclr, globals, 0);

	rp = join->rel;
	rp->present = 1;
	rp->pq = nil;
	m = 0;
	na = 0;

	while ((vec = nextline(join)) != nil) {
		if (vec[0][0] == '>') {
			if (strcmp(vec[0], ">") != 0) {
				pq_error(0, "join: relation %d: > must be followed by a space", RELNO(rp));
				free(join);
				return nil;
			}
			rp->mods[m] = nil;
			rp++;
			rp->present = 1;
			Vop(Vclr, rp->attrs, 0);
			Vop(Vclr, rp->keys, 0);
			Vop(Vclr, rp->deps, 0);
			rp->pq = nil;
			for (i = 1; vec[i] != nil; i++) {
				int global = 0, key = 0, lowpri = 0;
				char *attr;
				for (attr = vec[i]; *attr != 0 && !isalpha(*attr); attr++) {
					switch (*attr) {
					case '+':	key++;	break;
					case '*':	global++;	break;
					case '-':	lowpri++;	break;
					default:
						pq_error(0, "join: relation %d: %s: unknown modifier", RELNO(rp), vec[i]);
						free(join);
						return nil;
					}
				}
				if (*attr == 0) {
					if (strcmp(vec[i], "*") != 0 || join->defrel != nil) {
						pq_error(0, "join: relation %d: %s: bad token", RELNO(rp), vec[i]);
						free(join);
						return nil;
					}
					join->defrel = rp;
					continue;
				}
				for (j = 0; j < na; j++)
					if (eqattr(attr, join->attr[j]))
						break;
				if (j == na)
					join->attr[na++] = attr;
				Vop1(Vor, rp->attrs, j);
				if (key)
					Vop1(Vor, rp->keys, j);
				else
					Vop1(Vor, rp->deps, j);
				if (global)
					Vop1(Vor, globals, j);
				if (lowpri)
					Vop1(Vor, join->lowpri, j);
			}
			if (i == 1) {
				pq_error(0, "join: relation %d: > with no attributes (use *)", RELNO(rp));
				free(join);
				return nil;
			}
			m = 0;
		} else
			rp->mods[m++] = vec;
	}
	rp->mods[m] = nil;
	rp++;
	rp->present = 0;
	join->attr[na] = nil;

	rp = join->rel;
	if (rp->mods[0] != nil) {
		if ((rp+1)->present) {
			pq_error(0, "join: extra stuff before first > line");
			free(join);
			return nil;
		}
		if (!relopen(join, rp)) {
			free(join);
			return nil;
		}
		join->defrel = rp;
	}
	if (join->defrel != nil) {
		Vop(Vor, join->defrel->attrs, globals);
		Vop(Vor, join->defrel->deps, globals);
		Vop1(Vor, join->defrel->attrs, na);
		Vop1(Vor, join->defrel->deps, na);
	}
	return join;
}

int
join_close(void *vjoin)
{
	Join *join = vjoin;
	Rel *rp;
	int rv = 0;

	for (rp = join->rel; rp->present; rp++)
		if (rp->pq != nil && pq_close(rp->pq) == -1)
			rv = -1;
	free(join);
	return rv;
}

int
join_read(void *vjoin, char **argv)
{
	Join *join = vjoin;
	Qvec vec;
	int rv, i;
	Merge *mp;
	Query *qp;
	char *cp;

	if (join->version >= 0) {
		if (join->version > 0) {
			join->version--;
			argv[0] = VERSION;
			argv[1] = nil;
			return 1;
		}
		return 0;
	}
	if (join->list >= 0) {
		if (join->list > 0) {
			join->list--;
			argv[0] = join->attr[join->list];
			argv[1] = nil;
			return 1;
		}
		if (join->defrel == nil)
			return 0;
		if (!join->merge->wrote) {
			argv[0] = "attribute";
			argv[1] = nil;
			join->merge->wrote++;
			if (pqowrite(join, join->defrel, argv) == -1)
				return -1;
		}
		return pq_read(join->defrel->pq, argv);
	}

	if (join->rp != nil)
		return pq_read(join->rp->pq, argv);
	for (mp = join->merge; mp->rp != nil && mp->wrote; mp++)
		;
	if (mp->rp == nil)
		mp--;

	rv = -1;
	while (mp->rp != nil) {
		if (!mp->wrote) {
			cp = mp->base;
			i = 0;
			for (qp = join->query; qp->attr != nil; qp++) {
				if (Vop1(Vtst, mp->rp->attrs, qp->a)) {
					if (!Vop1(Vtst, mp->attrs, qp->a)) {
						vec[i++] = ++cp;
						cp = strcon(cp, join->attr[qp->a], "=", qp->val, nil);
					} else
						vec[i++] = qp->attr;
				}
			}
			vec[i] = nil;
			mp->wrote++;
			if (pqowrite(join, mp->rp, vec) == -1)
				return -1;
		} else if ((rv = pq_read(mp->rp->pq, vec)) > 0) {
			cp = mp->base;
			i = 0;
			for (qp = join->query; qp->attr != nil; qp++)
				if (Vop1(Vtst, mp->rp->attrs, qp->a))
					if (Vop1(Vtst, mp->attrs, qp->a)) {
						qp->val = ++cp;
						cp = strcon(cp, vec[i++], nil);
					} else
						i++;
			mp++;
			mp->base = cp;
		} else if (rv == 0) {
			if (mp == join->merge)
				return 0;
			mp->wrote = 0;
			mp--;
		} else
			return rv;
	}
	i = 0;
	for (qp = join->query; qp->attr != nil; qp++)
		argv[i++] = qp->val;
	argv[i] = nil;

	return rv;
}

int
join_write(void *vjoin, char **argv)
{
	Join *join = vjoin;
	char *cp, *attr, *val, *arg;
	Vec attrs, selected, projected, extra;
	Query *qp;
	Merge *mp;
	Rel *rp;
	int first, i, a;

	join->list = -1;
	join->version = -1;
	join->rp = nil;
	Vop(Vclr, attrs, 0);
	Vop(Vclr, selected, 0);

	cp = join->base;
	for (i = 0, qp = join->query; (arg = argv[i]) != nil; i++, qp++) {
		for (a = 0; (attr = join->attr[a]) != nil; a++)
			if (eqattr(arg, attr))
				break;
		if (eqattr(arg, "attribute")) {
			join->list = a;
			join->merge->wrote = 0;
			return 0;
		}
		if (eqattr(arg, "version")) {
			join->version = 1;
			return 0;
		}
		if (attr == nil && join->defrel == nil)
			return pq_error(0, "join %s: invalid attribute", arg);

		val = strchr(arg, '=');
		if (attr == nil || val != nil) {
			qp->attr = ++cp;
			cp = strcon(cp, arg, nil);
		} else
			qp->attr = attr;
		qp->val = nil;
		qp->a = a;
		Vop1(Vor, attrs, a);
		if (val != nil && !Vop1(Vtst, join->lowpri, a))
			Vop1(Vor, selected, a);
	}

	for (rp = join->rel; rp->present; rp++)
		if (!Vop(Vtnand, attrs, rp->attrs)) {
			if (debug > 0) {
				fprint(2, "(join) 1 relation\n");
				dump(join, rp, "for", attrs);
			}
			join->rp = rp;
			return pqowrite(join, join->rp, argv);
		}

rescan:
	for (rp = join->rel+1; rp->present; rp++)
		if (Vop(Vtand, attrs, rp->deps) && Vop(Vtnand, rp->keys, attrs)) {
			Vop(Vcpy, extra, rp->keys);
			Vop(Vnand, extra, attrs);
			for (i = 0; i < NBits; i++)
				if (Vop1(Vtst, extra, i)) {
					Vop1(Vor, attrs, i);
					qp->attr = join->attr[i];
					qp->val = nil;
					qp->a = i;
					qp++;
				}
			goto rescan;
		}

	Vop(Vcpy, projected, attrs);
	Vop(Vnand, projected, selected);
	if (!Vop(Vtst, selected, 0))
		Vop(Vcpy, selected, attrs);

	for (rp = join->rel+1; rp->present; rp++)
		rp->mark = 0;

	qp->attr = nil;
	mp = join->merge;
	mp->base = cp;
	first = 1;

	while (Vop(Vtst, attrs, 0)) {
		Rel* best = nil;
		int maxa = 0;
		for (rp = join->rel+1; rp->present; rp++) {
			if (!rp->mark && Vop(Vtand, selected, rp->attrs)) {
				if (Vop(Vtand, projected, rp->attrs)) {
					if (!first)
						break;
					Vop(Vcpy, extra, selected);
					Vop(Vor, extra, projected);
					Vop(Vand, extra, rp->attrs);
					a = Vsize(extra);
					if (a > maxa) {
						maxa = a;
						best = rp;
					}
				} else if (best == nil)
					best = rp;
			}
		}
		if (!rp->present) {
			if (best == nil)
				return pq_error(0, "cannot resolve query");
			rp = best;
		}
		if (first) {
			Vop(Vcpy, extra, selected);
			Vop(Vnand, extra, rp->attrs);
			Vop(Vor, projected, extra);
			Vop(Vand, selected, rp->attrs);
		}
		Vop(Vcpy, mp->attrs, attrs);
		Vop(Vand, mp->attrs, rp->attrs);
		Vop(Vnand, attrs, rp->attrs);
		Vop(Vor, selected,  mp->attrs);
		Vop(Vnand, projected, mp->attrs);
		mp->wrote = 0;
		mp->rp = rp;
		mp++;
		rp->mark++;
		first = 0;
	}
	mp->rp = nil;

	if (debug > 0) {
		fprint(2, "(join) %ld relations\n", mp - join->merge);
		for (mp = join->merge; mp->rp != nil; mp++)
			dump(join, mp->rp, "add", mp->attrs);
	}
	return 0;
}

static char**
nextline(Join *join)
{
	char *s = join->disptr;
	char **v = join->vecptr;
	int inword = 0;

	while (*s != 0) {
		switch (*s) {
		case '\'':
			*s++ = 0;
			*v++ = s;
			while (*s != 0 && *s != '\'')
				s++;
			if (*s != 0)
				*s++ = 0;
			inword = 0;
			break;
		case '#':
			*s++ = 0;
			while (*s != 0 && *s != '\n')
				s++;
			break;
		case ' ':
		case '\t':
			*s++ = 0;
			inword = 0;
			break;
		case '\n':
			*s++ = 0;
			inword = 0;
			if (v > join->vecptr) {
				char **rv = join->vecptr;
				*v++ = nil;
				join->vecptr = v;
				join->disptr = s;
				return rv;
			}
			break;
		case '\\':
			if (s[1] == '\n') {
				*s++ = 0;
				*s++ = 0;
				inword = 0;
				break;
			}
			/* fall through */
		default:
			if (!inword)
				*v++ = s;
			inword = 1;
			s++;
			break;
		}
	}
	return nil;	
}

static int
relopen(Join* join, Rel* rp)
{
	char **argv;
	int m;

	for (m = 0; (argv = rp->mods[m]) != nil; m++) {
		if (debug > 1) {
			char *vec[512];
			char relno[16];
			int i, v;

			v = 0;
			vec[v++] = "log";
			vec[v++] = "-t";
			sprint(relno, ">%-3ld", RELNO(rp));
			vec[v++] = relno;
			if (debug > 2)
				vec[v++] = "-f";
			for (i = 0; argv[i] != nil; i++)
				vec[v++] = argv[i];
			vec[v] = nil;
			argv = vec;
		}
		rp->pq = pq_open(argv);
		if (rp->pq != nil)
			return 1;
	}
	if (m == 0)
		pq_error(0, "join: relation %d: no modules", RELNO(rp));
	return 0;
}

static int
pqowrite(Join *join, Rel *rp, char **argv)
{
	if (rp->pq == nil && !relopen(join, rp))
		return -1;
	return pq_write(rp->pq, argv);
}

static int
Vop(Vcmd op, Vec dst, Vec src)
{
	int rv = 0, i;

	for (i = 0; i < NBits / BpB; i++)
		switch (op) {
		case Vtst:		rv |= dst[i];	break;
		case Vtand:	rv |= dst[i] & src[i];	break;
		case Vtnand:	rv |= dst[i] & ~src[i];	break;
		case Vclr:		dst[i] = 0;		break;
		case Vcpy:	dst[i] = src[i];	break;
		case Vor:		dst[i] |= src[i];	break;
		case Vand:	dst[i] &= src[i];	break;
		case Vnand:	dst[i] &= ~src[i];	break;
		}
	return rv;
}

static int
Vop1(Vcmd op, Vec dst, int src1)
{
	int rv = 0;
	int d = src1 / BpB;
	int b = 1 << (src1 % BpB);

	switch (op) {
	case Vtst:		rv = dst[d] & b;	break;
	case Vor:		dst[d] |= b;	break;
	}
	return rv;
}

static int
Vsize(Vec src)
{
	int i, n;

	n = 0;
	for (i = 0; i < NBits; i++)
		if (Vop1(Vtst, src, i))
			n++;
	return n;
}

static void
dump(Join* join, Rel* rp, char* id, Vec v)
{
	int i;

	fprint(2, "(>%-3ld) %s:", RELNO(rp), id);
	for (i = 0; i < NBits; i++)
		if (Vop1(Vtst, v, i)) {
			fprint(2, " ");
			if (Vop1(Vtst, join->lowpri, i))
				fprint(2, "-");
			if (Vop1(Vtst, rp->keys, i))
				fprint(2, "+");
			fprint(2, "%s", join->attr[i] != nil ? join->attr[i] : "*");
		}
	fprint(2, "\n");
}
