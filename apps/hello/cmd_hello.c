#include <stdio.h>
#include <stdlib.h>
#include "../../vendor/argtable3/argtable3.h"
#include "../../include/cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder — single source of truth for parsing and help output
 * -------------------------------------------------------------------------- */

static void build_hello_argtable(
    struct arg_lit  **help,
    struct arg_str  **name,
    struct arg_end  **end,
    void            **tbl)         /* caller-allocated array of 4 slots */
{
    *help = arg_lit0("h", "help", "show this help and exit");
    *name = arg_str0("n", "name", "NAME", "whom to greet (default: World)");
    *end  = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *name;
    tbl[2] = *end;
    tbl[3] = NULL;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void hello_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_str  *name;
    struct arg_end  *end;
    void            *tbl[4];

    build_hello_argtable(&help, &name, &end, tbl);

    fprintf(out, "Usage: hello ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nPrint a friendly greeting.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 3);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int hello_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    (void)in_stream;   /* hello never reads input */

    struct arg_lit  *help;
    struct arg_str  *name;
    struct arg_end  *end;
    void            *tbl[4];

    build_hello_argtable(&help, &name, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        hello_print_usage(out_stream);
        arg_freetable(tbl, 3);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "hello");
        fprintf(stderr, "Try 'hello --help' for more information.\n");
        arg_freetable(tbl, 3);
        return 1;
    }

    const char *whom = (name->count > 0) ? name->sval[0] : "World";
    fprintf(out_stream, "Hello, %s!\n", whom);

    arg_freetable(tbl, 3);
    return 0;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_hello_spec = {
    .name       = "hello",
    .summary    = "print a friendly greeting",
    .long_help  = "Print a greeting, optionally addressing a specific NAME. "
                  "Serves as the canonical reference implementation of the "
                  "cmd_spec_t anatomy.",
    .run         = hello_run,
    .print_usage = hello_print_usage,
};
