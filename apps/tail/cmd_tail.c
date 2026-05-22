#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_tail_argtable(
    struct arg_lit  **help,
    struct arg_int  **lines,
    struct arg_int  **bytes,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help  = arg_lit0("h", "help",  "show this help and exit");
    *lines = arg_int0("n", "lines", "NUM", "print last NUM lines instead of 10");
    *bytes = arg_int0("c", "bytes", "NUM", "print last NUM bytes");
    *json  = arg_lit0(NULL, "json", "emit machine-readable JSON (for agents/MCP)");
    *files = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to read (default: stdin)");
    *end   = arg_end(20);

    static void *tbl[7];
    tbl[0] = *help;
    tbl[1] = *lines;
    tbl[2] = *bytes;
    tbl[3] = *json;
    tbl[4] = *files;
    tbl[5] = *end;
    tbl[6] = NULL;

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

/* Read all lines from fp into a dynamically-allocated array.
 * Returns number of lines; caller frees lines[i] and lines. */
static int read_all_lines(FILE *fp, char ***lines_out)
{
    int    cap   = 64;
    int    count = 0;
    char **lines = malloc(cap * sizeof(char *));
    if (!lines) return -1;

    char   *buf = NULL;
    size_t  bufsz = 0;
    ssize_t len;

    while ((len = getline(&buf, &bufsz, fp)) != -1) {
        if (count == cap) {
            cap *= 2;
            char **tmp = realloc(lines, cap * sizeof(char *));
            if (!tmp) { free(buf); free(lines); return -1; }
            lines = tmp;
        }
        lines[count++] = strndup(buf, len);
    }
    free(buf);

    *lines_out = lines;
    return count;
}

static int tail_lines(FILE *fp, int n)
{
    char **lines = NULL;
    int    count = read_all_lines(fp, &lines);

    if (count < 0) { fprintf(stderr, "tail: out of memory\n"); return 1; }
    if (ferror(fp)) { /* fall through to free */ }

    int start = count - n;
    if (start < 0) start = 0;

    for (int i = start; i < count; i++)
        fputs(lines[i], stdout);

    for (int i = 0; i < count; i++) free(lines[i]);
    free(lines);
    return 0;
}

static int tail_bytes(FILE *fp, int n)
{
    /* Read entire stream, then print last n bytes. */
    char   *buf  = NULL;
    size_t  size = 0;
    size_t  cap  = 0;
    char    tmp[4096];
    size_t  got;

    while ((got = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (size + got + 1 > cap) {
            cap = (size + got + 1) * 2 + 4096;
            buf = realloc(buf, cap);
            if (!buf) { fprintf(stderr, "tail: out of memory\n"); return 1; }
        }
        memcpy(buf + size, tmp, got);
        size += got;
    }

    int err = ferror(fp);

    if (size > 0) {
        size_t start = (size_t)n < size ? size - (size_t)n : 0;
        fwrite(buf + start, 1, size - start, stdout);
    }

    free(buf);
    return err ? 1 : 0;
}

static int tail_json(FILE *fp, const char *label, int n_lines, int is_last)
{
    char **lines = NULL;
    int    count = read_all_lines(fp, &lines);

    if (count < 0) { fprintf(stderr, "tail: out of memory\n"); return 1; }

    int start = count - n_lines;
    if (start < 0) start = 0;

    /* Build content string */
    size_t total = 0, buf_cap = 0;
    char  *buf   = NULL;

    for (int i = start; i < count; i++) {
        size_t len = strlen(lines[i]);
        if (total + len + 1 > buf_cap) {
            buf_cap = (total + len + 1) * 2 + 4096;
            buf = realloc(buf, buf_cap);
            if (!buf) {
                fprintf(stderr, "tail: out of memory\n");
                for (int j = 0; j < count; j++) free(lines[j]);
                free(lines);
                return 1;
            }
        }
        memcpy(buf + total, lines[i], len);
        total += len;
    }

    for (int i = 0; i < count; i++) free(lines[i]);
    free(lines);

    printf("  {");
    if (label) {
        printf("\"file\": \"");
        json_escape(stdout, label, strlen(label));
        printf("\", ");
    }
    printf("\"content\": \"");
    if (buf) json_escape(stdout, buf, total);
    printf("\"}%s\n", is_last ? "" : ",");

    free(buf);
    return 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void tail_print_usage(FILE *out)
{
    struct arg_lit  *help, *json;
    struct arg_int  *lines, *bytes;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_tail_argtable(&help, &lines, &bytes, &json, &files, &end, &argtable);

    fprintf(out, "Usage: tail ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nPrint the last lines of each FILE to standard output.\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");

    arg_freetable(argtable, 6);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int tail_run(int argc, char **argv)
{
    struct arg_lit  *help, *json;
    struct arg_int  *lines, *bytes;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_tail_argtable(&help, &lines, &bytes, &json, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        tail_print_usage(stdout);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "tail");
        fprintf(stderr, "Try 'tail --help' for more information.\n");
        arg_freetable(argtable, 6);
        return 1;
    }

    int n_lines  = lines->count > 0 ? lines->ival[0] : 10;
    int n_bytes  = bytes->count > 0 ? bytes->ival[0] : -1;
    int use_json = json->count  > 0;
    int nfiles   = files->count;
    int ret      = 0;

    if (use_json) printf("[\n");

    if (nfiles == 0) {
        if (use_json)
            ret = tail_json(stdin, NULL, n_lines, 1);
        else if (n_bytes >= 0)
            ret = tail_bytes(stdin, n_bytes);
        else
            ret = tail_lines(stdin, n_lines);
    } else {
        for (int i = 0; i < nfiles; i++) {
            const char *path     = files->filename[i];
            int         is_stdin = strcmp(path, "-") == 0;
            FILE       *fp       = is_stdin ? stdin : fopen(path, "r");

            if (!fp) {
                fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
                ret = 1;
                continue;
            }

            if (nfiles > 1 && !use_json)
                printf("==> %s <==\n", path);

            if (use_json)
                ret |= tail_json(fp, is_stdin ? NULL : path, n_lines, i == nfiles - 1);
            else if (n_bytes >= 0)
                ret |= tail_bytes(fp, n_bytes);
            else
                ret |= tail_lines(fp, n_lines);

            if (!is_stdin) fclose(fp);
            if (nfiles > 1 && !use_json && i < nfiles - 1) putchar('\n');
        }
    }

    if (use_json) printf("]\n");

    arg_freetable(argtable, 6);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_tail_spec = {
    .name       = "tail",
    .summary    = "print the last lines of files",
    .long_help  = "Print the last NUM lines (default 10) of each FILE to standard output. "
                  "With multiple files, precede each with a filename header. "
                  "Use -c to limit by bytes instead of lines. "
                  "Use --json to emit structured JSON output.",
    .run         = tail_run,
    .print_usage = tail_print_usage,
};
