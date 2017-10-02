/*
 * Forge an empty record if none exists
 */

#include "all.h"

typedef struct {
	void*	tag;
	int	state;
	Qvec	attr, ret;
} Forge;

enum { First, Last, Normal };

static void
clean(Forge *forge, int all)
{
	char **sp;

	if (all) {
		for (sp = forge->attr; *sp != nil; sp++) {
			free(*sp);
			*sp = nil;
		}
	}
	for (sp = forge->ret; *sp != nil; sp++) {
		free(*sp);
		*sp = nil;
	}
}

void*
forge_open(char **argv)
{
	Forge *forge = malloc(sizeof (Forge));
	int i;

	forge->ret[0] = nil;
	for (i = 0; argv[i] != nil && strcmp(argv[i], "--") != 0; i++)
		forge->attr[i] = strdup(argv[i]);
	forge->attr[i] = nil;
	if (argv[i] == nil || argv[i+1] == nil) {
		pq_error(0, "forge: `--' screwup");
		clean(forge, 1);
		free(forge);
		return nil;
	}
	forge->tag = pq_open(argv+i+1);
	if (forge->tag == nil) {
		clean(forge, 1);
		free(forge);
		return nil;
	}
	forge->state = Normal;
	return forge;
}

int
forge_close(void *vforge)
{
	Forge *forge = vforge;
	int rv;

	rv = pq_close(forge->tag);
	clean(forge, 1);
	free(forge);
	return rv;
}

int
forge_read(void *vforge, char **argv)
{
	Forge *forge = vforge;
	int rv, i;

	if (forge->state == Last) {
		forge->state = Normal;
		return 0;
	}
	rv = pq_read(forge->tag, argv);
	if (forge->state == First) {
		if (rv == 0) {
			forge->state = Last;
			for (i = 0; forge->ret[i] != nil; i++)
				argv[i] = forge->ret[i];
			argv[i] = nil;
			rv = 1;
		} else
			forge->state = Normal;
	}
	return rv;
}

int
forge_write(void *vforge, char **argv)
{
	Forge *forge = vforge;
	char **sp;
	char *val;
	int isselected, isforged;
	int i;

	forge->state = First;
	for (i = 0; argv[i] != nil; i++) {
		for (sp = forge->attr; *sp != nil; sp++)
			if (eqattr(argv[i], *sp))
				break;
		/* make the logic clearer */
		isselected = (strchr(argv[i], '=') != nil);
		isforged = (*sp != nil);
		if (isselected == isforged) {
			forge->state = Normal;
			break;
		}
	}
	clean(forge, 0);
	if (forge->state == First) {
		for (i = 0; argv[i] != nil; i++) {
			val = strchr(argv[i], '=');
			forge->ret[i] = strdup(val != nil ? val+1 : "");
		}
		forge->ret[i] = nil;
	}
	return pq_write(forge->tag, argv);
}
