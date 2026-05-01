#include <stdio.h>
#include <string.h>
#include "registry.h"

#define REGISTRY_MAX 64

static cmd_spec_t *entries[REGISTRY_MAX];
static int count = 0;

void registry_register(cmd_spec_t *spec)
{
    if (count < REGISTRY_MAX)
        entries[count++] = spec;
}

cmd_spec_t *registry_find(const char *name)
{
    for (int i = 0; i < count; i++)
        if (strcmp(entries[i]->name, name) == 0)
            return entries[i];
    return NULL;
}

/* Print one-line summary for every registered command (used by 'help'). */
void registry_print_all(void)
{
    for (int i = 0; i < count; i++)
        printf("  %-16s %s\n", entries[i]->name, entries[i]->summary);
}
