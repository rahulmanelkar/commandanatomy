#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_sort_argtable(
    struct arg_lit  **help,
    struct arg_lit  **reverse,
    struct arg_lit  **numeric,
    struct arg_lit  **unique,
    struct arg_int  **key,
    struct arg_str  **sep,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help    = arg_lit0("h", "help",         "show this help and exit");
    *reverse = arg_lit0("r", "reverse",      "reverse the sort order");
    *numeric = arg_lit0("n", "numeric-sort", "compare fields as numeric values");
    *unique  = arg_lit0("u", "unique",       "output only the first of equal lines");
    *key     = arg_int0("k", "key",   "FIELD", "sort by FIELD (1-based column number)");
    *sep     = arg_str0("t", NULL,    "SEP",   "field separator (default: whitespace)");
    *json    = arg_lit0(NULL, "json",          "emit machine-readable JSON (for agents/MCP)");
    *files   = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to sort (default: stdin)");
    *end     = arg_end(20);

    static void *tbl[10];
    tbl[0] = *help;
    tbl[1] = *reverse;
    tbl[2] = *numeric;
    tbl[3] = *unique;
    tbl[4] = *key;
    tbl[5] = *sep;
    tbl[6] = *json;
    tbl[7] = *files;
    tbl[8] = *end;
    tbl[9] = NULL;

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

/* Comparator state (sort is single-threaded). */
static int  g_reverse;
static int  g_numeric;
static int  g_key;    /* 1-based; 0 = whole line */
static char g_sep;    /* '\0' = split on whitespace */

/* Return pointer to start of the Nth (1-based) field in line. */
static const char *get_field(const char *line, int field)
{
    if (field <= 0) return line;

    int f = 1;
    const char *p = line;

    if (g_sep == '\0') {
        while (*p == ' ' || *p == '\t') p++;
        while (f < field) {
            while (*p && *p != ' ' && *p != '\t') p++;
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) return line;
            f++;
        }
    } else {
        while (f < field) {
            p = strchr(p, g_sep);
            if (!p) return line;
            p++;
            f++;
        }
    }
    return p;
}

static int sort_cmp(const void *a, const void *b)
{
    const char *la = *(const char * const *)a;
    const char *lb = *(const char * const *)b;
    const char *sa = get_field(la, g_key);
    const char *sb = get_field(lb, g_key);

    int r;
    if (g_numeric) {
        double da = strtod(sa, NULL);
        double db = strtod(sb, NULL);
        r = (da > db) - (da < db);
    } else {
        r = strcmp(sa, sb);
    }
    return g_reverse ? -r : r;
}

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

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void sort_print_usage(FILE *out)
{
    struct arg_lit  *help, *reverse, *numeric, *unique, *json;
    struct arg_int  *key;
    struct arg_str  *sep;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_sort_argtable(&help, &reverse, &numeric, &unique, &key, &sep,
                        &json, &files, &end, &argtable);

    fprintf(out, "Usage: sort ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nSort lines of text files.\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-26s %s\n");

    arg_freetable(argtable, 9);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int sort_run(int argc, char **argv)
{
    struct arg_lit  *help, *reverse, *numeric, *unique, *json;
    struct arg_int  *key;
    struct arg_str  *sep;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_sort_argtable(&help, &reverse, &numeric, &unique, &key, &sep,
                        &json, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        sort_print_usage(stdout);
        arg_freetable(argtable, 9);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "sort");
        fprintf(stderr, "Try 'sort --help' for more information.\n");
        arg_freetable(argtable, 9);
        return 1;
    }

    g_reverse = reverse->count > 0;
    g_numeric = numeric->count > 0;
    int use_unique = unique->count > 0;
    g_key     = key->count > 0 ? key->ival[0] : 0;
    g_sep     = (sep->count > 0 && sep->sval[0][0]) ? sep->sval[0][0] : '\0';

    int use_json = json->count > 0;
    int nfiles   = files->count;

    /* Collect all lines from all inputs into one array. */
    char **lines = NULL;
    int    cap   = 0;
    int    total = 0;
    int    ret   = 0;

    #define APPEND_LINES(fp) do { \
        char **batch = NULL; \
        int n = read_all_lines(fp, &batch); \
        if (n < 0) { \
            fprintf(stderr, "sort: out of memory\n"); \
            arg_freetable(argtable, 9); return 1; \
        } \
        if (total + n > cap) { \
            cap = (total + n) * 2 + 64; \
            char **tmp = realloc(lines, cap * sizeof(char *)); \
            if (!tmp) { \
                fprintf(stderr, "sort: out of memory\n"); \
                free(batch); arg_freetable(argtable, 9); return 1; \
            } \
            lines = tmp; \
        } \
        memcpy(lines + total, batch, n * sizeof(char *)); \
        total += n; free(batch); \
    } while (0)

    if (nfiles == 0) {
        APPEND_LINES(stdin);
    } else {
        for (int i = 0; i < nfiles; i++) {
            const char *path     = files->filename[i];
            int         is_stdin = strcmp(path, "-") == 0;
            FILE       *fp       = is_stdin ? stdin : fopen(path, "r");
            if (!fp) {
                fprintf(stderr, "sort: %s: %s\n", path, strerror(errno));
                ret = 1;
                continue;
            }
            APPEND_LINES(fp);
            if (!is_stdin) fclose(fp);
        }
    }

    qsort(lines, total, sizeof(char *), sort_cmp);

    /* Mark duplicate lines as NULL when -u is active. */
    if (use_unique && total > 0) {
        for (int i = 1; i < total; i++) {
            char *prev = lines[i - 1];
            if (prev && sort_cmp(&lines[i], &prev) == 0) {
                free(lines[i]);
                lines[i] = NULL;
            }
        }
    }

    /* Count non-NULL entries for correct JSON comma placement. */
    int out_count = 0;
    for (int i = 0; i < total; i++) if (lines[i]) out_count++;

    if (use_json) printf("[\n");

    int printed = 0;
    for (int i = 0; i < total; i++) {
        if (!lines[i]) continue;
        size_t len = strlen(lines[i]);
        if (use_json) {
            size_t slen = (len > 0 && lines[i][len - 1] == '\n') ? len - 1 : len;
            printf("  \"");
            json_escape(stdout, lines[i], slen);
            printed++;
            printf("\"%s\n", printed == out_count ? "" : ",");
        } else {
            fwrite(lines[i], 1, len, stdout);
        }
        free(lines[i]);
    }

    if (use_json) printf("]\n");

    free(lines);
    arg_freetable(argtable, 9);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_sort_spec = {
    .name       = "sort",
    .summary    = "sort lines of text files",
    .long_help  = "Sort lines of text from FILE(s) (or stdin) and write to standard output. "
                  "Use -n for numeric sort, -r to reverse, -u to deduplicate. "
                  "Use -k FIELD and -t SEP to sort by a specific column. "
                  "Use --json to emit a JSON array of sorted strings.",
    .run         = sort_run,
    .print_usage = sort_print_usage,
};
