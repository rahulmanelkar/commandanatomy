#ifndef REGISTRY_H
#define REGISTRY_H

#include "../../include/cmd_spec.h"

void registry_register(cmd_spec_t *spec);
cmd_spec_t *registry_find(const char *name);
void registry_print_all(void);

#endif /* REGISTRY_H */
