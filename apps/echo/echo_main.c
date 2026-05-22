#include <stdlib.h>
#include "cmd_spec.h"

extern cmd_spec_t cmd_echo_spec;

int main(int argc, char **argv)
{
    return cmd_echo_spec.run(argc, argv);
}
