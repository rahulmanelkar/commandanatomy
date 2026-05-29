#include <stdio.h>
#include <stdlib.h>
#include "cmd_spec.h"

extern cmd_spec_t cmd_grep_spec;

int main(int argc, char **argv)
{
    return cmd_grep_spec.run(argc, argv, stdin, stdout);
}
