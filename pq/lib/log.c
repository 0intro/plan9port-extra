/*
 * Query logging module
 */

#include "all.h"
#include <pqsrv.h>

typedef struct {
	void*	tag;
	int	full;
	int	stamp;
	char*	mark;
	int	fd;
	Qbuf	buf;
} Log;

static void putstr(Log*, int, char*);
static char *dovec(Log*, char**);

void*
log_open(char **argv)
{
	Log *log = malloc(sizeof (Log));
	char *bp;
	char **ap;

	log->full = 0;
	log->stamp = 0;
	log->fd = 2;
	bp = log->buf;
	while (argv[0] != nil && argv[0][0] == '-') {
		switch (argv[0][1]) {
		case 'o':
			log->fd = open(argv[1], OWRITE);
			if (log->fd == -1) {
				pq_error(-1, "log: open %s", argv[1]);
				free(log);
				return nil;
			}
			argv += 2;
			break;
		case 't':
			bp += sprint(bp, "%s", argv[1]);
			argv += 2;
			break;
		case 'p':
			bp += sprint(bp, "%d", getpid());
			argv++;
			break;
		case 'f':
			log->full = 1;
			argv++;
			break;
		case 's':
			log->stamp = 1;
			argv++;
			break;
		default:
			pq_error(-1, "log: unknown option %s", argv[0]);
			free(log);
			return nil;
		}
	}
	if (bp == log->buf)
		for (ap = argv; *ap != nil; ap++)
			bp += sprint(bp, "%s%s", *ap, ap[1] != nil ? " " : "");
	*bp = 0;
	log->mark = strdup(log->buf);
	putstr(log, 0, "(open)");
	log->tag = pq_open(argv);
	if (log->tag == nil) {
		putstr(log, Serror, pq_errmsg);
		if (log->fd != 2)
			close(log->fd);
		free(log->mark);
		free(log);
		return nil;
	}
	return log;
}

int
log_close(void *vlog)
{
	Log *log = vlog;
	int rv;

	putstr(log, Sclose, "");
	rv = pq_close(log->tag);
	if (rv == -1)
		putstr(log, Serror, pq_errmsg);
	if (log->fd != 2)
		close(log->fd);
	free(log->mark);
	free(log);
	return rv;
}

int
log_read(void *vlog, char **argv)
{
	Log *log = vlog;
	int rv;

	putstr(log, Sread, "");
	rv = pq_read(log->tag, argv);
	if (rv > 0)
		putstr(log, 0, dovec(log, argv));
	else if (rv == 0)
		putstr(log, 0, "(nil)");
	else if (rv == -1)
		putstr(log, Serror, pq_errmsg);
	return rv;
}

int
log_write(void *vlog, char **argv)
{
	Log *log = vlog;
	int rv;

	putstr(log, Swrite, dovec(log, argv));
	rv = pq_write(log->tag, argv);
	if (rv == -1)
		putstr(log, Serror, pq_errmsg);
	return rv;
}

static void
putstr(Log *log, int c, char *msg)
{
	Qbuf buf;
	char *bp = buf;
	static char ch[2];

	if (log->full || c == Swrite) {
		if (log->stamp) {
			Tm *tm = localtime(time(nil));
			int tzhour = tm->tzoff / (60*60);
			int tzmin = (tm->tzoff / 60) % 60;
			if (tzmin < 0)
				tzmin = -tzmin;
			bp += sprint(bp, "%.4d.%.2d.%.2d=%.2d:%.2d:%.2d%+.2d%.2d ",
				tm->year+1900, tm->mon+1, tm->mday,
				tm->hour, tm->min, tm->sec, tzhour, tzmin);
		}
		ch[0] = c;
		bp += sprint(bp, "[%s] %s%s\n", log->mark, ch, msg);
		seek(log->fd, 0, 2);
		write(log->fd, buf, bp-buf);
	}
}

static char*
dovec(Log *log, char **argv)
{
	char *bp = log->buf;

	while (*argv != nil)
		bp += sprint(bp, "%c%s", Svalue, *argv++);
	return log->buf;
}
