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
    void            **tbl)        /* caller-allocated array of 7 slots */
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *number    = arg_lit0("n", "number",    "number all output lines");
    *show_ends = arg_lit0("E", "show-ends", "display $ at end of each line");
    *json      = arg_lit0(NULL, "json",     "emit machine-readable JSON (for agents/MCP)");
    *files     = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to concatenate (default: stdin)");
    *end       = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *number;
    tbl[2] = *show_ends;
    tbl[3] = *json;
    tbl[4] = *files;
    tbl[5] = *end;
    tbl[6] = NULL;
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

static int cat_plain(FILE *fp, FILE *out, int number, int show_ends)
{
    char   *line = NULL;
    size_t  cap  = 0;
    ssize_t len;
    long long lineno = 0;

    while ((len = getline(&line, &cap, fp)) != -1) {
        lineno++;
        if (number) fprintf(out, "%6lld\t", lineno);

        if (show_ends) {
            /* Strip trailing newline, print $, then newline. */
            ssize_t printlen = len;
            if (printlen > 0 && line[printlen - 1] == '\n') printlen--;
            fwrite(line, 1, printlen, out);
            fprintf(out, "$\n");
        } else {
            fwrite(line, 1, len, out);
        }
    }

    free(line);
    return ferror(fp) ? 1 : 0;
}

static int cat_json(FILE *fp, FILE *out, const char *label, int is_last)
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
            char *bigger = realloc(buf, cap);
            if (!bigger) {
                fprintf(stderr, "cat: out of memory\n");
                free(buf);                  /* old key still valid */
                return 1;
            }
            buf = bigger;
        }
        memcpy(buf + size, tmp, n);
        size += n;
    }

    int err = ferror(fp);

    fprintf(out, "  {");
    if (label) {
        fprintf(out, "\"file\": \"");
        json_escape(out, label, strlen(label));
        fprintf(out, "\", ");
    }
    fprintf(out, "\"content\": \"");
    if (buf) json_escape(out, buf, size);
    fprintf(out, "\"}%s\n", is_last ? "" : ",");

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
    void            *tbl[7];

    build_cat_argtable(&help, &number, &show_ends, &json, &files, &end, tbl);

    fprintf(out, "Usage: cat ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nConcatenate files and print to standard output.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 6);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int cat_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    struct arg_lit  *help, *number, *show_ends, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void            *tbl[7];

    build_cat_argtable(&help, &number, &show_ends, &json, &files, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        cat_print_usage(out_stream);
        arg_freetable(tbl, 6);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "cat");
        fprintf(stderr, "Try 'cat --help' for more information.\n");
        arg_freetable(tbl, 6);
        return 1;
    }

    int use_number    = number->count    > 0;
    int use_show_ends = show_ends->count > 0;
    int use_json      = json->count      > 0;
    int nfiles        = files->count;
    int ret           = 0;

    if (use_json) fprintf(out_stream, "[\n");

    if (nfiles == 0) {
        if (use_json)
            ret = cat_json(in_stream, out_stream, NULL, 1);
        else
            ret = cat_plain(in_stream, out_stream, use_number, use_show_ends);
    } else {
        for (int i = 0; i < nfiles; i++) {
            const char *path = files->filename[i];
            FILE *fp;
            int is_stdin = (strcmp(path, "-") == 0);

            fp = is_stdin ? in_stream : fopen(path, "rb");
            if (!fp) {
                fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
                ret = 1;
                continue;
            }

            if (use_json)
                ret |= cat_json(fp, out_stream, is_stdin ? NULL : path, i == nfiles - 1);
            else
                ret |= cat_plain(fp, out_stream, use_number, use_show_ends);

            if (!is_stdin) fclose(fp);
        }
    }

    if (use_json) fprintf(out_stream, "]\n");

    arg_freetable(tbl, 6);
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
