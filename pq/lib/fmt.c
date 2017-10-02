#include "all.h"

#define	CAPS	(1<<0)
#define	ALLCAPS	(1<<1)
#define	RJUST	(1<<2)
#define	LDEL	(1<<3)
#define	RDEL	(1<<4)

#define	SUBST	1
#define	LMARK	2
#define	RMARK	3

#define	FLAGS	0
#define	WIDTH	1
#define	PREC	2
#define	VAR	3
#define	PSIZE	4

void
fmtcomp(char *prog, char* fmt, char **vars)
{
	char c, b;
	char *p;
	int nl = 1, curly, i;

	vars[0] = 0;
	while (c = *fmt++)
	top:	switch (c) {
		case '%':
			*prog++ = SUBST;
			prog[FLAGS] = prog[WIDTH] = prog[PREC] = 0;
			p = &prog[WIDTH];
			curly = 0;
			while (c = *fmt++)
				switch (c) {
				case '^':	prog[FLAGS] |= CAPS;	break;
				case '+':	prog[FLAGS] |= ALLCAPS;	break;
				case '-':	prog[FLAGS] |= RJUST;	break;
				case '<':	prog[FLAGS] |= LDEL;	break;
				case '>':	prog[FLAGS] |= RDEL;	break;
				case '.':	p = &prog[PREC];		break;
				case '{':
					curly++;
					c = *fmt++;
					/* fall thru */
				default:
					if (isdigit(c)) {
						*p = (*p * 10) + (c - '0');
						break;
					}
					p = --fmt;
					while (isalnum(c))
						c = *++fmt;
					if (p == fmt) {
						prog--;
						goto brake;
					}
					if (c)
						*fmt++ = 0;
					for (i = 0; vars[i] &&
						strcmp(vars[i], p); i++);
					prog[VAR] = i;
					prog += PSIZE;
					if (!vars[i]) {
						vars[i] = p;
						vars[i+1] = 0;
					}
					if (c && !curly)
						goto top;
					goto brake;
				}
			prog--;
			fmt--;
		brake:	break;
		case '\\':
			switch (c = *fmt++) {
			case 'c':	nl = 0;		continue;
			case '<':	c = LMARK;	break;
			case '>':	c = RMARK;	break;
			case 'n':	c = '\n';		break;
			case 'r':	c = '\r';		break;
			case 't':	c = '\t';		break;
			case 'b':	c = '\b';		break;
			case 'f':	c = '\f';		break;
			case 'v':	c = '\v';		break;
			case 's':	c = ' ';		break;
			case 0:	c = '\\';
				fmt--;
				break;
			default:
				for (i = b = 0; i < 3 && isdigit(c); i++) {
					b = (b * 010) + (c - '0');
					c = *fmt++;
				}
				if (i > 0) {
					c = b;
					fmt--;
				}
			}
			/* fall thru */
		default:
			*prog++ = c;
		}
	if (nl) {
		*prog++ = RMARK;
		*prog++ = '\n';
	}
	*prog = 0;
}

int
fmtexec(char *out, char *prog, char **vals)
{
	int len, spaces;
	char c, prev;
	char *val;
	char *mark = out;
	char *start = out;

	while (c = *prog++)
		switch (c) {
		case SUBST:
			c = prog[FLAGS];
			val = vals[prog[VAR]];
			if ((len = strlen(val)) == 0 && c & (LDEL|RDEL)) {
				prog += PSIZE;
				if (c & LDEL)
					out = mark;
				if (c & RDEL)
					while (*prog && *prog != RMARK)
						if (*prog++ == SUBST)
							prog += PSIZE;
				break;
			}
			if (prog[PREC] > 0 && prog[PREC] < len)
				len = prog[PREC];
			spaces = prog[WIDTH] - len;
			if (spaces > 0 && c & RJUST)
				while (spaces--)
					*out++ = ' ';
			prev = 0;
			while (len--)
				*out++ = prev = c & ALLCAPS ||
					       (c & CAPS && !isalnum(prev))
						? toupper(*val++) : *val++;
			if (spaces > 0 && !(c & RJUST))
				while (spaces--)
					*out++ = ' ';
			prog += PSIZE;
			break;
		case LMARK:
			mark = out;
			break;
		case RMARK:
			break;
		default:
			*out++ = c;
		}
	*out = 0;
	return out-start;
}
