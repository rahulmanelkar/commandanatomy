#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_cat_argtable(
    struct arg_lit  **help,
    struct arg_lit  **number,
    struct arg_lit  **show_ends,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *number    = arg_lit0("n", "number",    "number all output lines");
    *show_ends = arg_lit0("E", "show-ends", "display $ at end of each line");
    *json      = arg_lit0(NULL, "json",     "emit machine-readable JSON (for agents/MCP)");
    *files     = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to concatenate (default: stdin)");
    *end       = arg_end(20);

    static void *tbl[7];
    tbl[0] = *help;
    tbl[1] = *number;
    tbl[2] = *show_ends;
    tbl[3] = *json;
    tbl[4] = *files;
    tbl[5] = *end;
    tbl[6] = NULL;

    *argtable_out = tbl;
}

/* --------------------------------------------------------------------------
 * helpers
 * -------------------------------------------------------------------------- */

/* Escape a string for JSON: backslash, double-quote, and control characters. */
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
 * stream processing
 * -------------------------------------------------------------------------- */

static int cat_plain(FILE *fp, int number, int show_ends)
{
    char   *line = NULL;
    size_t  cap  = 0;
    ssize_t len;
    long long lineno = 0;

    while ((len = getline(&line, &cap, fp)) != -1) {
        lineno++;
        if (number) printf("%6lld\t", lineno);

        if (show_ends) {
            /* Strip trailing newline, print $, then newline. */
            ssize_t printlen = len;
            if (printlen > 0 && line[printlen - 1] == '\n') printlen--;
            fwrite(line, 1, printlen, stdout);
            printf("$\n");
        } else {
            fwrite(line, 1, len, stdout);
        }
    }

    free(line);
    return ferror(fp) ? 1 : 0;
}

static int cat_json(FILE *fp, const char *label, int is_last)
{
    /* Read entire stream into a buffer, then emit as a JSON object. */
    char   *buf  = NULL;
    size_t  size = 0;
    size_t  cap  = 0;
    char    tmp[4096];
    size_t  n;

    while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (size + n + 1 > cap) {
            cap = (size + n + 1) * 2 + 4096;
            buf = realloc(buf, cap);
            if (!buf) { fprintf(stderr, "cat: out of memory\n"); return 1; }
        }
        memcpy(buf + size, tmp, n);
        size += n;
    }

    int err = ferror(fp);

    printf("  {");
    if (label) {
        printf("\"file\": \"");
        json_escape(stdout, label, strlen(label));
        printf("\", ");
    }
    printf("\"content\": \"");
    if (buf) json_escape(stdout, buf, size);
    printf("\"}%s\n", is_last ? "" : ",");

    free(buf);
    return err ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void cat_print_usage(FILE *out)
{
    struct arg_lit  *help, *number, *show_ends, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cat_argtable(&help, &number, &show_ends, &json, &files, &end, &argtable);

    fprintf(out, "Usage: cat ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nConcatenate files and print to standard output.\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");

    arg_freetable(argtable, 6);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int cat_run(int argc, char **argv)
{
    struct arg_lit  *help, *number, *show_ends, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cat_argtable(&help, &number, &show_ends, &json, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        cat_print_usage(stdout);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "cat");
        fprintf(stderr, "Try 'cat --help' for more information.\n");
        arg_freetable(argtable, 6);
        return 1;
    }

    int use_number    = number->count    > 0;
    int use_show_ends = show_ends->count > 0;
    int use_json      = json->count      > 0;
    int nfiles        = files->count;
    int ret           = 0;

    if (use_json) printf("[\n");

    if (nfiles == 0) {
        if (use_json)
            ret = cat_json(stdin, NULL, 1);
        else
            ret = cat_plain(stdin, use_number, use_show_ends);
    } else {
        for (int i = 0; i < nfiles; i++) {
            const char *path = files->filename[i];
            FILE *fp;
            int is_stdin = (strcmp(path, "-") == 0);

            fp = is_stdin ? stdin : fopen(path, "rb");
            if (!fp) {
                fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
                ret = 1;
                continue;
            }

            if (use_json)
                ret |= cat_json(fp, is_stdin ? NULL : path, i == nfiles - 1);
            else
                ret |= cat_plain(fp, use_number, use_show_ends);

            if (!is_stdin) fclose(fp);
        }
    }

    if (use_json) printf("]\n");

    arg_freetable(argtable, 6);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_cat_spec = {
    .name       = "cat",
    .summary    = "concatenate files and print to standard output",
    .long_help  = "Concatenate FILE(s) and write to standard output. "
                  "Reads standard input when no files are given or when FILE is '-'. "
                  "Use --json to emit file contents as structured JSON.",
    .run         = cat_run,
    .print_usage = cat_print_usage,
};
