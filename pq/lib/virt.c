/*
 * Virtual attributes translation module
 */

#include "all.h"

#define	MAXVEC	512
#define	BUF	2048
#define	SMBUF	512

typedef struct {
	char*	tag;
	char	type[MAXVEC];
	char	flast[MAXVEC];
	char	buf[BUF];
	char	list;
	int	name_ind;
} Virt;

#define	TELR	0		/* "reversed" tel	*/
#define	TELS	1

static char	*tel[] = { "telr" };

#define	FIRST	0
#define	MIDDLE	1
#define	RPREFIX	2
#define	LAST	3
#define	SUFFIX	4
#define	SOUNDEX	5
#define	NAMES	6

static char	last[] = "last";
static char	flast[] = "flast";
static char	*name[] = { "first", "middle", "rprefix", last,
			    "suffix", "soundex" };

#define	FAXR	0		/* "reversed" fax	*/
#define	FAXS	1

static char	*fax[] = { "faxr" };

#define	PGRR	0		/* "reversed" pgr	*/
#define	PGRS	1

static char	*pgr[] = { "pgrr" };

#define	CELR	0		/* "reversed" cel	*/
#define	CELS	1

static char	*cel[] = { "celr" };

#define	SDTELR	0		/* "reversed" sdtel	*/
#define	SDTELS	1

static char	*sdtel[] = { "sdtelr" };

#define	SDFAXR	0		/* "reversed" sdfax	*/
#define	SDFAXS	1

static char	*sdfax[] = { "sdfaxr" };

#define	SDTEL2R	0		/* "reversed" sdtel2	*/
#define	SDTEL2S	1

static char	*sdtel2[] = { "sdtel2r" };

#define	TEL	0
#define	NAME	1
#define	PN	2
#define	FAX	3
#define	PGR	4
#define	CEL	5
#define	SDTEL	6
#define	SDFAX	7
#define	SDTEL2	8
#define	VIRTS	9

static struct {
	char*	cmp;
	char**	exp;
	int	len;
	int	type;
} attr[] = {
	"tel",		tel,	TELS,		0,
	"name",		name,	NAMES,		1,
	"pn",		name,	NAMES,		1,
	"fax",		fax,	FAXS,		0,
	"pgr",		pgr,	PGRS,		0,
	"cel",		cel,	CELS,		0,
	"sdtel",	sdtel,	SDTELS,		0,
	"sdfax",	sdfax,	SDFAXS,		0,
	"sdtel2",	sdtel2,	SDTEL2S,	0
};

static int	hopt = 0;	/* hopt = 0, default no handle	*/
static int	args = 0;

static char *compress(char*, char**, int, int);
static int expand(char*, char**, int);
static char *soundex(char*);

void*
virt_open(char **argv)
{
	Virt *virt = malloc(sizeof (Virt));

	if (!strcmp(argv[0], "-h")) {
		hopt = 1;
		args++;
	}
	if ((virt->tag = pq_open(argv + args)) == 0) {
		free(virt);
		return 0;
	}
	memset(virt->type, VIRTS, sizeof virt->type);
	virt->list = 0;
	virt->name_ind = 0;
	return virt;
}

int
virt_close(void *vvirt)
{
	Virt *virt = vvirt;
	int rv = pq_close(virt->tag);
	free(virt);
	return rv;
}

int
virt_read(void *vvirt, char **argv)
{
	Virt *virt = vvirt;
	char *bp = virt->buf;
	int rv, i, j, k;
	char *vec[MAXVEC];

	if (virt->list) {
		argv[0] = attr[--virt->list].cmp;
		argv[1] = 0;
		return 1;
	}
	if ((rv = pq_read(virt->tag, vec)) > 0) {
		for (i = j = 0; vec[i]; i++)
			if ((k = virt->type[i]) < VIRTS) {
				argv[j++] = bp;
				bp = compress(bp, &vec[i], k, virt->flast[i])
					+ 1;
				i += attr[k].len - 1;
			} else
				argv[j++] = vec[i];
		argv[j] = 0;
	}
	return rv;
}

int
virt_write(void *vvirt, char **argv)
{
	Virt *virt = vvirt;
	char *bp = virt->buf;
	int i, j, k, n, spec, lst;
	char *vec[MAXVEC];
	char buf[SMBUF];
	char *s, *t;
	char temp[SMBUF];

	virt->list = 0;
	for (i = j = 0; argv[i]; i++) {
		if (eqattr(argv[i], "attribute"))
			virt->list = VIRTS;
		for (k = 0; (virt->type[j] = k) < VIRTS; k++) {
			if (eqattr(argv[i], attr[k].cmp)) {
				for (n = 0; n < attr[k].len; n++)
					vec[j+n] = 0;
				*(t = virt->flast + j) = 0;
				if (s = strchr(argv[i], '=')) {
					if (attr[k].type) virt->name_ind++;
					*t = expand(strcpy(buf, ++s),
						vec+j, k);
				}
			/* the hack starts here */
				if ((k == TEL || k == FAX || k == PGR || 
				     k == CEL || k == SDTEL || k == SDFAX || 
				     k == SDTEL2) && s && *t == 2) {
					/*
					 * we didn't reverse the tel/fax value,
					 * but we stuck +1 in front of it, so
					 * now we have to get that into the
					 * argument vector, but arrange things
					 * so that virt_read doesn't mess with
					 * it later
					 */
					virt->type[j] = VIRTS;
					s = vec[j];
					vec[j++] = bp;
					bp = strcon(bp, attr[k].cmp, "=", s,
						(char *)0) + 1;
					break;
				}
			/* and it ends here	*/
				if ((k == TEL || k == FAX || k == PGR ||
				     k == CEL || k == SDTEL || k == SDFAX || 
				     k == SDTEL2) && (!s || *t))
					continue; /* will cause k=VIRTS	*/
				name[LAST] = *t ? flast : last;
				for (n = 0; n < attr[k].len; n++)
					if (s = vec[j+n]) {
						vec[j+n] = bp;
						bp = strcon(bp, attr[k].exp[n],
							"=", s, (char *)0) + 1;
					} else
						vec[j+n] = attr[k].exp[n];
				j += n;
				break;
			}
		}
		if (k == VIRTS)
			vec[j++] = argv[i];
	}
	vec[j] = 0;
	for (i = 0, spec = 0, lst = -1;
	     i < j && virt->name_ind && hopt;
	     ++i) {
		if (!strncmp(vec[i], "first=", strlen("first=")) ||
		    !strncmp(vec[i], "middle=", strlen("middle=")) ||
		    !strncmp(vec[i], "rprefix=", strlen("rprefix=")) ||
		    !strncmp(vec[i], "suffix=", strlen("suffix=")))
			if (++spec > 1) break;
		if (!strncmp(vec[i], "last=", strlen("last="))) lst = i;
	}
	if (!spec && virt->name_ind && hopt && lst >= 0) {
		s = strchr(vec[lst], '=');
		strcpy(temp, "id");
		strcat(temp, s);
		*s = 0;
		vec[j] = temp;
		vec[j + 1] = '\0';
	}
	return pq_write(virt->tag, vec);
}

static int
expand(char *s, char **v, int type)
{
	static char buf[SMBUF];
	char *t;
	int i;

	if (type == TEL || type == FAX || type == PGR || type == CEL ||
	    type == SDTEL || type == SDFAX || type == SDTEL2) {
		register char	*b = buf;
		while (*s && isspace(*s))    /* skip leading whitespace	*/
			++s;
		/*
		 * if the tel/fax starts with '+', assume the user wants
		 * exact or prefix match on tel/fax, and don't change the
		 * given value into a telr/faxr
		 */
		if (*s == '+')
			return 1;
		if((i = strlen(s)) < 1)
			return 1; /* if nothing but spaces, don't change */
		t = s + i - 1;	/* last character in the string	*/
		while (t > s && isspace(*t)) /* skip trailing whitespace */
			--t;
		if (t == s && *t == '*')
			return 1;     /* if nothing but *, don't change	*/
		/*
		 * if the tel/fax doesn't end in *, reverse the given value
		 * to make this a search on telr/faxr
		 */
		if (*t != '*') {
			while ((t >= s) && (b - buf < SMBUF - 1))
				*b++ = *t--;
			*b = 0;
			v[0] = buf;
			return 0;
		}
		/*
		 * At this point, we've determined that the value contains
		 * something other than spaces, it ends in '*', and is not
		 * just an '*', so it's going to be treated as a tel or fax.
		 * However, since it also doesn't start with '+', it isn't
		 * going to match anything because all "tel"s and "fax"s in
		 * the database start with a country code.  We'll put "1"
		 * in front of the given value, if it doesn't already have
		 * one, and hope for the best.
		 */
		if (*s != '1') *b++ = '1';
		while (*s && (b - buf < SMBUF - 1))
			*b++ = *s++;
		*b = 0;
		v[0] = buf;
		return 2;
	} else {
		int	sdx = 0;

		if ((t = strrchr(s, '_')) && t > s && *--t == '_'
		 || (t = strrchr(s, ','))) {
			if (*t == '_')
				*t++ = 0;
			*t++ = 0;
			v[SUFFIX] = t;
		}
		if (*s == '?') {
			s++, sdx++;
		}
		i = strlen(s);
		if (i >= 1 && strchr("?~", s[i-1]))
			sdx++;
		if (i >= 3 && !strcmp(s+i-3, "..."))
			strcpy(s+i-3, "*");
		for (i = FIRST; (v[i] = s) && i < LAST; i++)
			if (s = strpbrk(s, "_."))
				*s++ = 0;
		if (!v[LAST]) {
			v[LAST] = v[--i];
			v[i] = 0;
		}
		if (sdx) {
			v[SOUNDEX] = soundex(v[LAST]);
			v[LAST] = 0;
		}
		if (v[FIRST] && v[LAST]) {
			for (t = v[FIRST]; *t && !isalnum(*t); t++);
			if (*t) {
				buf[0] = *t;
				strcpy(buf+1, v[LAST]);
				v[LAST] = buf;
				return 1;
			}
		}
	}
	return 0;
}

static char *
compress(char *s, char **v, int type, int flast)
{
	int i;

	if (type == TEL || type == FAX || type == PGR || type == CEL ||
	    type == SDTEL || type == SDFAX || type == SDTEL2) {
		register char	*t = v[0] + strlen(v[0]);
		while (t > v[0]) /* reverse the telr/faxr value	*/
			*s++ = *--t;
		*s = 0;
	} else {
		if (flast)
			v[LAST]++;
		if (type == PN)
			s = strcon(s, v[LAST], ",", (char *) 0);
		for (i = FIRST; i <= (type==PN ? RPREFIX : LAST); i++)
			s = strcon(s, (i==FIRST || !*v[i]) ? "" :
				type==PN ? " " : "_", v[i], (char *) 0);
		if (*v[SUFFIX])
			s = strcon(s, type==PN ? "," : "__",
				v[SUFFIX], (char *) 0);
	}
	return s;
}

#define SDXLEN  4

static char *
soundex(char *name)
{
        static char sdx[SDXLEN+1];
        char c, lc, prev = '0';
        int i;

        strcpy(sdx, "a000");
        for (i = 0; i < SDXLEN && *name; name++)
                if (isalpha(*name)) {
                        lc = tolower(*name);
                        c = "01230120022455012623010202"[lc-'a'];
                        if (i == 0 || (c != '0' && c != prev)) {
                                sdx[i] = i ? c : lc;
                                i++;
                        }
                        prev = c;
                }
        return sdx;
}
