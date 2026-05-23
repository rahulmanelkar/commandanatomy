#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_tee_argtable(
    struct arg_lit  **help,
    struct arg_lit  **append,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help   = arg_lit0("h", "help",   "show this help and exit");
    *append = arg_lit0("a", "append", "append to files instead of overwriting");
    *json   = arg_lit0(NULL, "json",  "emit machine-readable JSON (for agents/MCP)");
    *files  = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to write to (in addition to stdout)");
    *end    = arg_end(20);

    static void *tbl[6];
    tbl[0] = *help;
    tbl[1] = *append;
    tbl[2] = *json;
    tbl[3] = *files;
    tbl[4] = *end;
    tbl[5] = NULL;

    *argtable_out = tbl;
}

/* --------------------------------------------------------------------------
 * helpers
 * -------------------------------------------------------------------------- */

static void json_escape(FILE *out, const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        if      (ch == '"')  fputs("\\\"", out);
        else if (ch == '\\') fputs("\\\\", out);
        else if (ch == '\n') fputs("\\n",  out);
        else if (ch == '\r') fputs("\\r",  out);
        else if (ch == '\t') fputs("\\t",  out);
        else if (ch < 0x20)  fprintf(out, "\\u%04x", ch);
        else                 fputc(ch, out);
    }
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void tee_print_usage(FILE *out)
{
    struct arg_lit  *help, *append, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_tee_argtable(&help, &append, &json, &files, &end, &argtable);

    fprintf(out, "Usage: tee ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nRead from stdin and write to stdout and files simultaneously.\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");

    arg_freetable(argtable, 5);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int tee_run(int argc, char **argv)
{
    struct arg_lit  *help, *append, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_tee_argtable(&help, &append, &json, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        tee_print_usage(stdout);
        arg_freetable(argtable, 5);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "tee");
        fprintf(stderr, "Try 'tee --help' for more information.\n");
        arg_freetable(argtable, 5);
        return 1;
    }

    int use_append = append->count > 0;
    int use_json   = json->count   > 0;
    int nfiles     = files->count;

    const char *mode = use_append ? "a" : "w";

    /* Open output files. */
    FILE **fps = NULL;
    if (nfiles > 0) {
        fps = malloc(nfiles * sizeof(FILE *));
        if (!fps) {
            fprintf(stderr, "tee: out of memory\n");
            arg_freetable(argtable, 5);
            return 1;
        }
    }

    int ret = 0;
    for (int i = 0; i < nfiles; i++) {
        fps[i] = fopen(files->filename[i], mode);
        if (!fps[i]) {
            fprintf(stderr, "tee: %s: %s\n", files->filename[i], strerror(errno));
            fps[i] = NULL;
            ret = 1;
        }
    }

    /* Read all of stdin, write to stdout and all open files. */
    if (use_json) {
        /* Buffer everything, then emit JSON. */
        char   *buf  = NULL;
        size_t  size = 0, cap = 0;
        char    tmp[4096];
        size_t  got;

        while ((got = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
            if (size + got + 1 > cap) {
                cap = (size + got + 1) * 2 + 4096;
                buf = realloc(buf, cap);
                if (!buf) {
                    fprintf(stderr, "tee: out of memory\n");
                    ret = 1; break;
                }
            }
            memcpy(buf + size, tmp, got);
            size += got;

            /* Still write raw to files in real time. */
            for (int i = 0; i < nfiles; i++)
                if (fps[i]) fwrite(tmp, 1, got, fps[i]);
        }

        printf("{\"bytes_read\": %zu, \"content\": \"", size);
        if (buf) json_escape(stdout, buf, size);
        printf("\"}\n");
        free(buf);

    } else {
        char   buf[4096];
        size_t got;

        while ((got = fread(buf, 1, sizeof(buf), stdin)) > 0) {
            fwrite(buf, 1, got, stdout);
            for (int i = 0; i < nfiles; i++)
                if (fps[i]) fwrite(buf, 1, got, fps[i]);
        }
    }

    for (int i = 0; i < nfiles; i++)
        if (fps[i]) fclose(fps[i]);
    free(fps);

    arg_freetable(argtable, 5);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_tee_spec = {
    .name       = "tee",
    .summary    = "read from stdin and write to stdout and files",
    .long_help  = "Read from standard input and write simultaneously to standard output "
                  "and each FILE. Use -a to append rather than overwrite. "
                  "Useful in pipelines where you want to capture intermediate output. "
                  "Use --json to emit a summary object with the captured content.",
    .run         = tee_run,
    .print_usage = tee_print_usage,
};
