#include <stdlib.h>
#include "cmd_spec.h"

extern cmd_spec_t cmd_head_spec;

int main(int argc, char **argv)
{
    return cmd_head_spec.run(argc, argv);
}
