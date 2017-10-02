#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <pq.h>
#include "config.h"

typedef char*	Qvec[PQ_VEC];
typedef short	Qref[PQ_VEC];
typedef char	Qbuf[PQ_REC];

typedef struct {
	char*	name;
	void*	(*open)(char**);
	int	(*close)(void*);
	int	(*read)(void*, char**);
	int	(*write)(void*, char**);
} Module;

typedef struct {
	int	cur, max;
	void	**ptrs;
} GC;

extern Module	modtab[];

int	cnt(char**);
void*	gcalloc(GC*, int);
void	gcfree(GC*);
char*	gcdup(GC*, char*);
