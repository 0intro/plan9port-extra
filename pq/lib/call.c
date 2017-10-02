/*
 * Call server module
 */

#include "all.h"
#include <pqsrv.h>

#define	DEFSVR	"x.bell-labs.com"

typedef struct {
	int	fd, tee;
	Qbuf	line, buf;
	char*	ptr;
	char*	end;
} Call;

static int	stalk(Call*, int, char*);
static int	sgetc(Call*);

void*
call_open(char **argv)
{
	Call *call = malloc(sizeof(Call));

	call->tee = -1;
	if (argv[0] != nil && strcmp(argv[0], "-t") == 0) {
		call->tee = open(argv[1], OWRITE);
		if (call->tee == -1) {
			pq_error(-1, "call: open: tee %s", argv[1]);
			free(call);
			return nil;
		}
		argv += 2;
	}
	if (argv[0] == nil)
		argv[0] = DEFSVR;
	call->fd = dial(netmkaddr(argv[0], "tcp", "411"), 0, 0, 0);
	if (call->fd == -1) {
		pq_error(-1, "call: open: dial %s", argv[0]);
		free(call);
		return nil;
	}
	call->ptr = call->end = call->buf;
	if (stalk(call, 0, "") == -1) {
		free(call);
		return nil;
	}
	return call;
}

int
call_close(void *vcall)
{
	Call *call = vcall;
	int r1, r2;

	r1 = stalk(call, Sclose, "");
	if (call->tee != -1)
		close(call->tee);
	r2 = close(call->fd);
	if (r2 < 0)
		pq_error(-1, "call: close");
	free(call);
	if (r1 < 0 || r2 < 0)
		return -1;
	else
		return 0;
}

int
call_read(void *vcall, char **argv)
{
	Call *call = vcall;
	char *r, *w;
	int rv;

	rv = stalk(call, Sread, "");
	if (rv > 0) {
		r = w = call->line;
		while (*r != 0) {
			switch (*w = *r++) {
			case Svalue:
				*w++ = 0;
				*argv++ = w;
				break;
			case '\\':
				if (*r != 0) {
					if (*r == 'n')
						*r = '\n';
					*w++ = *r++;
				}
				break;
			default:
				w++;
				break;
			}
		}
		*argv = nil;
	}
	return rv;
}

int
call_write(void *vcall, char **argv)
{
	Call *call = vcall;
	char *a, *s;
	Qbuf line;

	s = line;
	while ((a = *argv++) != nil) {
		*s++ = Svalue;
		while ((*s++ = *a++) != 0)
			;
		s--;
	}
	return stalk(call, Swrite, line);
}

static int
stalk(Call *call, int cmd, char *arg)
{
	char *s;
	int n, c;

	if (cmd != 0) {
		s = call->line;
		*s++ = cmd;
		if (arg != nil) {
			while ((*s++ = *arg++) != 0)
				;
			s--;
		}
		*s++ = '\n';
		n = s - call->line;
		if (write(call->fd, call->line, n) != n)
			return pq_error(-1, "call: write");
		if (call->tee != -1)
			write(call->tee, call->line, n);
	}
	c = sgetc(call);
	switch (c) {
	case Sprompt:
		return 0;
	case '\n':
	case Svalue:
	case Serror:
		s = call->line;
		while (c != -1 && c != '\n') {
			*s++ = c;
			c = sgetc(call);
		}
		if (c == -1)
			return -1;
		*s = 0;
		if (sgetc(call) == Sprompt) {
			if (*call->line == Serror)
				return pq_error(0, "%s", call->line + 1);
			else
				return 1;
		}
		/* fall thru */
	default:
		if (c == -1)
			return -1;
		return pq_error(0, "call: Protocol error: %c", c);
	}
}

static int
sgetc(Call *call)
{
	if (call->ptr == call->end) {
		int n = read(call->fd, call->buf, sizeof call->buf);
		if (n == 0)
			return pq_error(0, "call: read: end of file");
		if (n < 0)
			return pq_error(1, "call: read");
		if (call->tee != -1)
			write(call->tee, call->buf, n);
		call->ptr = call->buf;
		call->end = call->buf + n;
	}
	return *call->ptr++ & 0xFF;
}
