#define	PQ_VEC	512
#define	PQ_REC	4096

void*	pq_open(char**);
int	pq_close(void*);
int	pq_read(void*, char**);
int	pq_write(void*, char**);
int	pq_error(int, char*, ...);
char*	pq_path(char*);

void	fmtcomp(char*, char*, char**);
int	fmtexec(char*, char*, char**);
int	eqattr(char*, char*);
char*	strcon(char*, ...);
int	strvec(char*, char**, char*);
int	strvecc(char*, char**, char);

extern char	pq_errmsg[];

#pragma	lib	"/sys/src/cmd/pq/lib/libpq.a"
