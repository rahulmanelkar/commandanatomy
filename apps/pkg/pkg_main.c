#include <stdio.h>
#include "cmd_spec.h"

extern cmd_spec_t cmd_pkg_spec;

int main(int argc, char **argv)
{
    return cmd_pkg_spec.run(argc, argv, stdin, stdout);
}
