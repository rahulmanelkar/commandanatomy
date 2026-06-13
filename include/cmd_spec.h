#ifndef CMD_SPEC_H
#define CMD_SPEC_H

#include <stdio.h>

typedef struct cmd_spec {
    const char *name;
    const char *summary;
    const char *long_help;
    int  (*run)(int argc, char **argv, FILE *in_stream, FILE *out_stream);
    void (*print_usage)(FILE *out);

    /* Optional MCP hook. Populate the caller-supplied tbl[] (capacity maxn)
     * with this command's argtable3 arg objects WITHOUT freeing them, and
     * return the number of entries written (including the arg_end terminator),
     * or -1 if maxn is too small. The caller owns the objects and releases them
     * with arg_freetable(tbl, n). This lets the MCP layer derive a typed
     * inputSchema from the SAME argtable the parser and --help already use, with
     * all allocation kept on the caller's stack (thread-safe). NULL = the
     * command exposes no machine-readable schema. */
    int  (*build_argtable)(void **tbl, int maxn);
} cmd_spec_t;

#endif /* CMD_SPEC_H */
