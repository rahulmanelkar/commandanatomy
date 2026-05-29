#include <stdio.h>
#include <stdlib.h>
#include "../../include/cmd_spec.h"
#include "registry.h"

extern cmd_spec_t cmd_hello_spec;

int main(int argc, char **argv)
{
    registry_register(&cmd_hello_spec);
    return cmd_hello_spec.run(argc, argv, stdin, stdout);
}
