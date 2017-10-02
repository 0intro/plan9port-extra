/*
 * Switch module
 */

#include "all.h"

typedef struct {
	Module*	mod;
	void*	tag;
	int	list;
} Pq;

void*
pq_open(char **argv)
{
	Pq *pq = malloc(sizeof (Pq));
	static char *defv[] = { "opt", "join", nil };

	if (argv == nil || argv[0] == nil)
		argv = defv;
	pq->list = -1;
	for (pq->mod = modtab; pq->mod->name != nil; pq->mod++)
		if (strcmp(argv[0], pq->mod->name) == 0)
			break;
	if (pq->mod->name == nil) {
		pq_error(0, "pq %s: Module not configured", argv[0]);
		free(pq);
		return nil;
	}
	pq->tag = (*pq->mod->open)(argv+1);
	if (pq->tag == nil) {
		free(pq);
		return nil;
	}
	return pq;
}

int
pq_close(void *vpq)
{
	Pq *pq = vpq;
	int rv;

	if (pq == nil)
		return pq_error(0, "pq: close: Not opened");
	rv = (pq->mod->close)(pq->tag);
	free(pq);
	return rv;
}

int
pq_read(void *vpq, char **arg)
{
	Pq *pq = vpq;

	if (pq == nil)
		return pq_error(0, "pq: read: Not opened");
	if (pq->list >= 0) {
		arg[0] = modtab[pq->list++].name;
		if (arg[0] == nil) {
			pq->list = -1;
			return 0;
		}
		return 1;
	}
	return (*pq->mod->read)(pq->tag, arg);
}

int
pq_write(void *vpq, char **arg)
{
	Pq *pq = vpq;

	if (pq == nil)
		return pq_error(0, "pq: write: Not opened");
	if (arg != nil && eqattr(arg[0], "modules") && arg[1] == nil) {
		pq->list = 0;
		return 0;
	} else
		pq->list = -1;
	return (*pq->mod->write)(pq->tag, arg);
}
