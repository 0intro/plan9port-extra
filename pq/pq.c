#include <u.h>
#include <libc.h>
#include <bio.h>
#include <pq.h>

typedef char*	strings[PQ_VEC];
typedef char	buffer[32*1024];

int	dodef(strings, strings);
void	pqerr(void);
void	usage(void);

Biobuf	bo;
int	rv = 0;
buffer	prog, out, arg;


void
main(int argc, char **argv)
{
	char *ofmt = nil, *attr = "default", *mods = "", *seps = "|";
	strings vars, vals, defs;
	int v, i, rw, rr, c;
	int version = 0;
	void *pq;

	Binit(&bo, 1, OWRITE);
	argv0 = argv[0];
	ARGBEGIN {
	case 'V':
		ofmt = "%version";
		version++;
		break;
	case 'o':
		ofmt = ARGF();
		break;
	case 'a':
		attr = ARGF();
		break;
	case 'm':
		mods = ARGF();
		break;
	case 's':
		seps = ARGF();
		break;
	case 'd':
		sprint(arg, "join -d %s", ARGF());
		mods = arg;
		break;
	default:
		usage();
	} ARGEND
	if (version)
		argv[argc++] = "";
	if (argc == 0 || ofmt == nil)
		usage();
	strvec(mods, vals, " \t\n");
	pq = pq_open(vals);
	if (pq == nil) {
		pqerr();
		exits(pq_errmsg);
	}
	fmtcomp(prog, ofmt, vars);
	for (v = 0; vars[v] != nil; v++)
		;
	strvec(attr, defs, seps);
	for (i = 0; i < argc; i++) {
		strcpy(arg, argv[i]);
		strvec(arg, vars+v, seps);
		if (!dodef(vars+v, defs))
			continue;
		rw = pq_write(pq, vars);
		if (rw != -1) {
			for (c = 0; (rr = pq_read(pq, vals)) > 0; c++)
				Bwrite(&bo, out, fmtexec(out, prog, vals));
			if (rr == -1)
				pqerr();
			else if (c == 0) {
				Bflush(&bo);
				fprint(2, "%s: %s: not found\n", argv0, argv[i]);
				strcpy(pq_errmsg, "no records");
				rv++;
			}
		}
		if (rw == -1)
			pqerr();
	}
	if (pq_close(pq) == -1)
		pqerr();
	if (rv != 0)
		exits(pq_errmsg);
	exits(nil);
}

int
dodef(strings vars, strings defs)
{
	static buffer buf;
	char *bp = buf;
	int m, i, n;

	for (m = 0; defs[m] != nil; m++)
		;
	for (i = 0; vars[i] != nil; i++) {
		if (strchr(vars[i], '=') == nil) {
			if (i < m) {
				n = sprint(bp, "%s=%s", defs[i], vars[i]);
				vars[i] = bp;
				bp += n+1;
			} else {
				Bflush(&bo);
				fprint(2, "%s: %s: no -a attribute\n", argv0, vars[i]);
				return 0;
			}
		}
	}
	return 1;
}

void
pqerr(void)
{
	Bflush(&bo);
	fprint(2, "%s: %s\n", argv0, pq_errmsg);
	rv++;
}

void
usage(void)
{
	sysfatal("usage: -o fmt [-V] [-a attrs] [-m modules] [-s seps] [-d debug] query...");
}
