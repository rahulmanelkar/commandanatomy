#include <stdio.h>
#include <stdlib.h>
#include "cmd_spec.h"

extern cmd_spec_t cmd_stat_spec;

int main(int argc, char **argv)
{
    return cmd_stat_spec.run(argc, argv, stdin, stdout);
}
