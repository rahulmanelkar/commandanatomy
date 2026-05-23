#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_cut_argtable(
    struct arg_lit  **help,
    struct arg_str  **fields,
    struct arg_str  **chars,
    struct arg_str  **delim,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help   = arg_lit0("h", "help",        "show this help and exit");
    *fields = arg_str0("f", "fields", "LIST", "select fields (e.g. 1,3 or 1-3,5)");
    *chars  = arg_str0("c", "characters", "LIST", "select character positions");
    *delim  = arg_str0("d", "delimiter", "DELIM", "field delimiter (default: TAB)");
    *json   = arg_lit0(NULL, "json",       "emit machine-readable JSON (for agents/MCP)");
    *files  = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to cut (default: stdin)");
    *end    = arg_end(20);

    static void *tbl[8];
    tbl[0] = *help;
    tbl[1] = *fields;
    tbl[2] = *chars;
    tbl[3] = *delim;
    tbl[4] = *json;
    tbl[5] = *files;
    tbl[6] = *end;
    tbl[7] = NULL;

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

#define MAX_POSITIONS 4096

/* Parse a list like "1,3-5,7" into a bool array (1-based, index 0 unused).
 * Returns max position seen, or -1 on parse error. */
static int parse_list(const char *spec, char selected[MAX_POSITIONS])
{
    memset(selected, 0, MAX_POSITIONS);
    int maxpos = 0;
    const char *p = spec;

    while (*p) {
        char *endp;
        long a = strtol(p, &endp, 10);
        if (endp == p || a < 1 || a >= MAX_POSITIONS) return -1;
        long b = a;

        if (*endp == '-') {
            p = endp + 1;
            b = strtol(p, &endp, 10);
            if (endp == p || b < a || b >= MAX_POSITIONS) return -1;
        }
        for (long i = a; i <= b; i++) selected[i] = 1;
        if (b > maxpos) maxpos = (int)b;

        p = endp;
        if (*p == ',') p++;
    }
    return maxpos;
}

/* Cut fields from line using delimiter. Outputs selected fields separated by delim. */
static void cut_fields(const char *line, size_t len, const char selected[MAX_POSITIONS],
                       char delim, FILE *out, int use_json)
{
    /* Trim trailing newline for processing, remember if it existed. */
    int has_nl = (len > 0 && line[len - 1] == '\n');
    size_t dlen = has_nl ? len - 1 : len;

    int   field   = 1;
    int   printed = 0;
    const char *p = line;
    const char *end = line + dlen;

    while (p <= end) {
        const char *next = memchr(p, delim, end - p);
        const char *fend = next ? next : end;
        size_t flen = fend - p;

        if (field < MAX_POSITIONS && selected[field]) {
            if (printed > 0) {
                if (use_json) fputs("\\t", out);
                else fputc(delim, out);
            }
            if (use_json)
                json_escape(out, p, flen);
            else
                fwrite(p, 1, flen, out);
            printed++;
        }

        field++;
        p = next ? next + 1 : end + 1;
    }
}

/* Cut character positions from line. */
static void cut_chars(const char *line, size_t len, const char selected[MAX_POSITIONS],
                      FILE *out, int use_json)
{
    int has_nl = (len > 0 && line[len - 1] == '\n');
    size_t dlen = has_nl ? len - 1 : len;

    for (size_t i = 0; i < dlen; i++) {
        int pos = (int)(i + 1);
        if (pos < MAX_POSITIONS && selected[pos]) {
            if (use_json)
                json_escape(out, line + i, 1);
            else
                fputc(line[i], out);
        }
    }
}

static int cut_stream(FILE *fp, const char selected[MAX_POSITIONS], char delim,
                      int do_fields, int use_json, const char *label, int is_last)
{
    char   *line = NULL;
    size_t  cap  = 0;
    ssize_t len;
    int     first_line = 1;

    if (use_json) {
        printf("  {");
        if (label) { printf("\"file\": \""); json_escape(stdout, label, strlen(label)); printf("\", "); }
        printf("\"lines\": [\n");
    }

    while ((len = getline(&line, &cap, fp)) != -1) {
        if (use_json) {
            if (!first_line) printf(",\n");
            printf("    \"");
            if (do_fields)
                cut_fields(line, len, selected, delim, stdout, 1);
            else
                cut_chars(line, len, selected, stdout, 1);
            printf("\"");
            first_line = 0;
        } else {
            if (do_fields)
                cut_fields(line, len, selected, delim, stdout, 0);
            else
                cut_chars(line, len, selected, stdout, 0);
            putchar('\n');
        }
    }

    free(line);

    if (use_json) {
        if (!first_line) putchar('\n');
        printf("  ]}%s\n", is_last ? "" : ",");
    }

    return ferror(fp) ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void cut_print_usage(FILE *out)
{
    struct arg_lit  *help, *json;
    struct arg_str  *fields, *chars, *delim;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cut_argtable(&help, &fields, &chars, &delim, &json, &files, &end, &argtable);

    fprintf(out, "Usage: cut ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nRemove sections from each line of files.\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-28s %s\n");
    fprintf(out, "\nLIST format: comma-separated numbers or ranges, e.g. 1,3-5,7\n");

    arg_freetable(argtable, 7);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int cut_run(int argc, char **argv)
{
    struct arg_lit  *help, *json;
    struct arg_str  *fields, *chars, *delim;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cut_argtable(&help, &fields, &chars, &delim, &json, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        cut_print_usage(stdout);
        arg_freetable(argtable, 7);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "cut");
        fprintf(stderr, "Try 'cut --help' for more information.\n");
        arg_freetable(argtable, 7);
        return 1;
    }

    if (fields->count == 0 && chars->count == 0) {
        fprintf(stderr, "cut: you must specify -f or -c\n");
        arg_freetable(argtable, 7);
        return 1;
    }
    if (fields->count > 0 && chars->count > 0) {
        fprintf(stderr, "cut: cannot use -f and -c together\n");
        arg_freetable(argtable, 7);
        return 1;
    }

    char selected[MAX_POSITIONS];
    int  do_fields = fields->count > 0;
    const char *list_str = do_fields ? fields->sval[0] : chars->sval[0];

    if (parse_list(list_str, selected) < 0) {
        fprintf(stderr, "cut: invalid field/position list: %s\n", list_str);
        arg_freetable(argtable, 7);
        return 1;
    }

    char sep = '\t';
    if (delim->count > 0 && delim->sval[0][0])
        sep = delim->sval[0][0];

    int use_json = json->count > 0;
    int nfiles   = files->count;
    int ret      = 0;

    if (use_json) printf("[\n");

    if (nfiles == 0) {
        ret = cut_stream(stdin, selected, sep, do_fields, use_json, NULL, 1);
    } else {
        for (int i = 0; i < nfiles; i++) {
            const char *path     = files->filename[i];
            int         is_stdin = strcmp(path, "-") == 0;
            FILE       *fp       = is_stdin ? stdin : fopen(path, "r");
            if (!fp) {
                fprintf(stderr, "cut: %s: %s\n", path, strerror(errno));
                ret = 1;
                continue;
            }
            ret |= cut_stream(fp, selected, sep, do_fields, use_json,
                              is_stdin ? NULL : path, i == nfiles - 1);
            if (!is_stdin) fclose(fp);
        }
    }

    if (use_json) printf("]\n");

    arg_freetable(argtable, 7);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_cut_spec = {
    .name       = "cut",
    .summary    = "remove sections from each line of files",
    .long_help  = "Print selected fields (-f) or character positions (-c) from each line. "
                  "Use -d to set the field delimiter (default: TAB). "
                  "LIST is a comma-separated sequence of numbers or ranges (e.g. 1,3-5,7). "
                  "Use --json to emit structured JSON output.",
    .run         = cut_run,
    .print_usage = cut_print_usage,
};
