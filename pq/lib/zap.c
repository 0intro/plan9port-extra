/*
 * Zap ssn/pid attributes module
 */

#include "all.h"

void*
zap_open(char **argv)
{
	return pq_open(argv);
}

int
zap_close(void *vzap)
{
	return pq_close(vzap);
}

int
zap_read(void *vzap, char **argv)
{
	return pq_read(vzap, argv);
}

int
zap_write(void *vzap, char **argv)
{
	int select = 0, project = 0;
	int i;

	for (i = 0; argv[i] != nil; i++) {
		if (eqattr(argv[i], "ssn") || eqattr(argv[i], "pid"))
			if (strchr(argv[i], '=') != nil)
				select++;
			else
				project++;
	}
	if (project && !select)
		return pq_error(0, "ssn/pid: restricted access");
	return pq_write(vzap, argv);
}
