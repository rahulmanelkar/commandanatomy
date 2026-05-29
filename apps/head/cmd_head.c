#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_head_argtable(
    struct arg_lit  **help,
    struct arg_int  **lines,
    struct arg_int  **bytes,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void            **tbl)         /* caller-allocated array of 7 slots */
{
    *help  = arg_lit0("h", "help",  "show this help and exit");
    *lines = arg_int0("n", "lines", "NUM", "print first NUM lines instead of 10");
    *bytes = arg_int0("c", "bytes", "NUM", "print first NUM bytes");
    *json  = arg_lit0(NULL, "json", "emit machine-readable JSON (for agents/MCP)");
    *files = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to read (default: stdin)");
    *end   = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *lines;
    tbl[2] = *bytes;
    tbl[3] = *json;
    tbl[4] = *files;
    tbl[5] = *end;
    tbl[6] = NULL;
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

static int head_lines(FILE *fp, FILE *out, int n)
{
    char   *line = NULL;
    size_t  cap  = 0;
    ssize_t len;
    int     count = 0;

    while (count < n && (len = getline(&line, &cap, fp)) != -1) {
        fwrite(line, 1, len, out);
        count++;
    }

    free(line);
    return ferror(fp) ? 1 : 0;
}

static int head_bytes(FILE *fp, FILE *out, int n)
{
    char buf[4096];
    int  remaining = n;

    while (remaining > 0) {
        int    want = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        size_t got  = fread(buf, 1, want, fp);
        if (got == 0) break;
        fwrite(buf, 1, got, out);
        remaining -= (int)got;
    }

    return ferror(fp) ? 1 : 0;
}

static int head_json(FILE *fp, FILE *out, const char *label, int n_lines, int is_last)
{
    char   *line = NULL;
    size_t  cap  = 0;
    ssize_t len;
    int     count   = 0;
    size_t  total   = 0;
    size_t  buf_cap = 0;
    char   *buf     = NULL;

    while (count < n_lines && (len = getline(&line, &cap, fp)) != -1) {
        if (total + (size_t)len + 1 > buf_cap) {
            buf_cap = (total + (size_t)len + 1) * 2 + 4096;
            buf = realloc(buf, buf_cap);
            if (!buf) {
                fprintf(stderr, "head: out of memory\n");
                free(line);
                return 1;
            }
        }
        memcpy(buf + total, line, len);
        total += len;
        count++;
    }
    free(line);

    int err = ferror(fp);

    fprintf(out, "  {");
    if (label) {
        fprintf(out, "\"file\": \"");
        json_escape(out, label, strlen(label));
        fprintf(out, "\", ");
    }
    fprintf(out, "\"content\": \"");
    if (buf) json_escape(out, buf, total);
    fprintf(out, "\"}%s\n", is_last ? "" : ",");

    free(buf);
    return err ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void head_print_usage(FILE *out)
{
    struct arg_lit  *help, *json;
    struct arg_int  *lines, *bytes;
    struct arg_file *files;
    struct arg_end  *end;
    void            *tbl[7];

    build_head_argtable(&help, &lines, &bytes, &json, &files, &end, tbl);

    fprintf(out, "Usage: head ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nPrint the first lines of each FILE to standard output.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 6);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int head_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    struct arg_lit  *help, *json;
    struct arg_int  *lines, *bytes;
    struct arg_file *files;
    struct arg_end  *end;
    void            *tbl[7];

    build_head_argtable(&help, &lines, &bytes, &json, &files, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        head_print_usage(out_stream);
        arg_freetable(tbl, 6);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "head");
        fprintf(stderr, "Try 'head --help' for more information.\n");
        arg_freetable(tbl, 6);
        return 1;
    }

    int n_lines  = lines->count > 0 ? lines->ival[0] : 10;
    int n_bytes  = bytes->count > 0 ? bytes->ival[0] : -1;
    int use_json = json->count  > 0;
    int nfiles   = files->count;
    int ret      = 0;

    if (use_json) fprintf(out_stream, "[\n");

    if (nfiles == 0) {
        if (use_json)
            ret = head_json(in_stream, out_stream, NULL, n_lines, 1);
        else if (n_bytes >= 0)
            ret = head_bytes(in_stream, out_stream, n_bytes);
        else
            ret = head_lines(in_stream, out_stream, n_lines);
    } else {
        for (int i = 0; i < nfiles; i++) {
            const char *path     = files->filename[i];
            int         is_stdin = strcmp(path, "-") == 0;
            FILE       *fp       = is_stdin ? in_stream : fopen(path, "r");

            if (!fp) {
                fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
                ret = 1;
                continue;
            }

            if (nfiles > 1 && !use_json)
                fprintf(out_stream, "==> %s <==\n", path);

            if (use_json)
                ret |= head_json(fp, out_stream, is_stdin ? NULL : path, n_lines, i == nfiles - 1);
            else if (n_bytes >= 0)
                ret |= head_bytes(fp, out_stream, n_bytes);
            else
                ret |= head_lines(fp, out_stream, n_lines);

            if (!is_stdin) fclose(fp);
            if (nfiles > 1 && !use_json && i < nfiles - 1) fputc('\n', out_stream);
        }
    }

    if (use_json) fprintf(out_stream, "]\n");

    arg_freetable(tbl, 6);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_head_spec = {
    .name       = "head",
    .summary    = "print the first lines of files",
    .long_help  = "Print the first NUM lines (default 10) of each FILE to standard output. "
                  "With multiple files, precede each with a filename header. "
                  "Use -c to limit by bytes instead of lines. "
                  "Use --json to emit structured JSON output.",
    .run         = head_run,
    .print_usage = head_print_usage,
};
