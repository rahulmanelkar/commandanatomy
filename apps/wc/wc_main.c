#include <stdlib.h>
#include "cmd_spec.h"

extern cmd_spec_t cmd_wc_spec;

int main(int argc, char **argv)
{
    return cmd_wc_spec.run(argc, argv);
}
