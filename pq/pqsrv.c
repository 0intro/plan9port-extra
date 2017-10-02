/*
 * Server side of protocol
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <pq.h>
#include <pqsrv.h>

Biobuf	bi, bo;
void	perr(void);

void
main(int argc, char **argv)
{
	void *pq;
	char *line;
	char *vec[PQ_VEC];
	char **v, *s, c;
	int rv;

	USED(argc);
	Binit(&bi, 0, OREAD);
	Binit(&bo, 1, OWRITE);
	pq = pq_open(&argv[1]);
	if (pq == nil)
		perr();

	for (;;) {
		BPUTC(&bo, Sprompt);
		Bflush(&bo);
		line = Brdline(&bi, '\n');
		if (line == nil || line[0] == ('d' & 037))	/* EOF or ctrl-d */
			break;
		if (line[0] == '\n')
			continue;
		if (pq == nil) {
			pq_error(0, "server: not open");
			perr();
			continue;
		}
		switch (line[0]) {
		case Sread:
			rv = pq_read(pq, vec);
			if (rv > 0) {
				for (v = vec; *v != nil; v++) {
					BPUTC(&bo, Svalue);
					for (s = *v; (c = *s) != 0; s++) {
						if (strchr("\\|\n", c) != nil) {
							BPUTC(&bo, '\\');
							if (c == '\n')
								c = 'n';
						}
						BPUTC(&bo, c);
					}
				}
				BPUTC(&bo, '\n');
			} else if (rv == -1)
				perr();
			break;
		case Swrite:
			line[BLINELEN(&bi)-1] = 0;
			s = line;
			v = vec;
			while ((s = strchr(s, Svalue)) != nil) {
				*s++ = 0;
				*v++ = s;
			}
			*v = nil;
			if (pq_write(pq, vec) == -1)
				perr();
			break;
		case Sclose:
			if (pq_close(pq) == -1)
				perr();
			pq = nil;
			break;
		default:
			pq_error(0, "server: %c: unknown command", line[0]);
			perr();
			break;
		}
	}
	if (pq != nil)
		pq_close(pq);
}

void
perr(void)
{
	Bprint(&bo, "%c%s\n", Serror, pq_errmsg);
}
