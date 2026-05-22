#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_grep_argtable(
    struct arg_lit  **help,
    struct arg_lit  **ignore_case,
    struct arg_lit  **line_number,
    struct arg_lit  **count,
    struct arg_lit  **invert,
    struct arg_lit  **files_with,
    struct arg_lit  **json,
    struct arg_str  **pattern,
    struct arg_file **files,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help       = arg_lit0("h", "help",              "show this help and exit");
    *ignore_case= arg_lit0("i", "ignore-case",       "ignore case distinctions in PATTERN");
    *line_number= arg_lit0("n", "line-number",       "prefix each output line with its line number");
    *count      = arg_lit0("c", "count",             "print only a count of matching lines per file");
    *invert     = arg_lit0("v", "invert-match",      "select non-matching lines");
    *files_with = arg_lit0("l", "files-with-matches","print only names of files with matches");
    *json       = arg_lit0(NULL,"json",              "emit machine-readable JSON (for agents/MCP)");
    *pattern    = arg_str1(NULL, NULL, "PATTERN",    "regular expression to search for");
    *files      = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to search (default: stdin)");
    *end        = arg_end(20);

    static void *tbl[11];
    tbl[0]  = *help;
    tbl[1]  = *ignore_case;
    tbl[2]  = *line_number;
    tbl[3]  = *count;
    tbl[4]  = *invert;
    tbl[5]  = *files_with;
    tbl[6]  = *json;
    tbl[7]  = *pattern;
    tbl[8]  = *files;
    tbl[9]  = *end;
    tbl[10] = NULL;

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

typedef struct {
    int  line_number;
    int  count;
    int  invert;
    int  files_with;
    int  use_json;
    int  show_filename;
    regex_t *re;
} grep_opts;

/* Returns number of matching lines (or -1 on error). */
static int grep_stream(FILE *fp, const char *label, grep_opts *opts,
                       int is_last_file)
{
    char   *line  = NULL;
    size_t  cap   = 0;
    ssize_t len;
    long    lineno = 0;
    int     matches = 0;

    /* For JSON + files_with or count, we collect first, then emit once. */
    typedef struct { long lineno; char *text; size_t tlen; } match_t;
    match_t *matched   = NULL;
    int      match_cap = 0;

    while ((len = getline(&line, &cap, fp)) != -1) {
        /* Strip trailing newline for matching (keep for output). */
        size_t print_len = (size_t)len;
        lineno++;

        int hit = (regexec(opts->re, line, 0, NULL, 0) == 0);
        if (opts->invert) hit = !hit;
        if (!hit) continue;

        matches++;

        if (opts->use_json) {
            if (!opts->count && !opts->files_with) {
                if (matches > match_cap) {
                    match_cap = match_cap ? match_cap * 2 : 16;
                    matched = realloc(matched, match_cap * sizeof(*matched));
                    if (!matched) { fprintf(stderr, "grep: out of memory\n"); free(line); return -1; }
                }
                matched[matches - 1].lineno = lineno;
                matched[matches - 1].text   = strndup(line, print_len);
                matched[matches - 1].tlen   = print_len;
            }
        } else {
            if (opts->count || opts->files_with)
                continue;

            if (opts->show_filename) printf("%s:", label ? label : "(stdin)");
            if (opts->line_number)   printf("%ld:", lineno);
            fwrite(line, 1, print_len, stdout);
            if (print_len == 0 || line[print_len - 1] != '\n') putchar('\n');
        }
    }

    free(line);

    if (opts->use_json) {
        if (opts->count) {
            printf("  {");
            if (label) { printf("\"file\": \""); json_escape(stdout, label, strlen(label)); printf("\", "); }
            printf("\"count\": %d}%s\n", matches, is_last_file ? "" : ",");
        } else if (opts->files_with) {
            if (matches > 0) {
                printf("  \"");
                json_escape(stdout, label ? label : "(stdin)", strlen(label ? label : "(stdin)"));
                printf("\"%s\n", is_last_file ? "" : ",");
            }
        } else {
            printf("  {");
            if (label) { printf("\"file\": \""); json_escape(stdout, label, strlen(label)); printf("\", "); }
            printf("\"matches\": [\n");
            for (int i = 0; i < matches; i++) {
                printf("    {\"line\": %ld, \"text\": \"", matched[i].lineno);
                json_escape(stdout, matched[i].text, matched[i].tlen);
                printf("\"}%s\n", i == matches - 1 ? "" : ",");
                free(matched[i].text);
            }
            printf("  ]}%s\n", is_last_file ? "" : ",");
            free(matched);
        }
    } else {
        if (opts->count) {
            if (opts->show_filename) printf("%s:", label ? label : "(stdin)");
            printf("%d\n", matches);
        } else if (opts->files_with && matches > 0) {
            printf("%s\n", label ? label : "(stdin)");
        }
    }

    return matches;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void grep_print_usage(FILE *out)
{
    struct arg_lit  *help, *ignore_case, *line_number, *count, *invert, *files_with, *json;
    struct arg_str  *pattern;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_grep_argtable(&help, &ignore_case, &line_number, &count, &invert,
                        &files_with, &json, &pattern, &files, &end, &argtable);

    fprintf(out, "Usage: grep ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nSearch for PATTERN in each FILE (or stdin).\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-28s %s\n");

    arg_freetable(argtable, 10);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int grep_run(int argc, char **argv)
{
    struct arg_lit  *help, *ignore_case, *line_number, *count, *invert, *files_with, *json;
    struct arg_str  *pattern;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_grep_argtable(&help, &ignore_case, &line_number, &count, &invert,
                        &files_with, &json, &pattern, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        grep_print_usage(stdout);
        arg_freetable(argtable, 10);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "grep");
        fprintf(stderr, "Try 'grep --help' for more information.\n");
        arg_freetable(argtable, 10);
        return 1;
    }

    int re_flags = REG_EXTENDED | (ignore_case->count > 0 ? REG_ICASE : 0);
    regex_t re;
    int rc = regcomp(&re, pattern->sval[0], re_flags);
    if (rc != 0) {
        char errbuf[256];
        regerror(rc, &re, errbuf, sizeof(errbuf));
        fprintf(stderr, "grep: invalid pattern: %s\n", errbuf);
        arg_freetable(argtable, 10);
        return 2;
    }

    grep_opts opts = {
        .line_number  = line_number->count > 0,
        .count        = count->count       > 0,
        .invert       = invert->count      > 0,
        .files_with   = files_with->count  > 0,
        .use_json     = json->count        > 0,
        .show_filename = files->count > 1,
        .re            = &re,
    };

    int nfiles      = files->count;
    int total_match = 0;
    int ret         = 0;

    if (opts.use_json) {
        if (opts.files_with)
            printf("[\n");
        else
            printf("[\n");
    }

    if (nfiles == 0) {
        int m = grep_stream(stdin, NULL, &opts, 1);
        if (m < 0) ret = 2;
        else if (m == 0) ret = 1;
        total_match += m < 0 ? 0 : m;
    } else {
        for (int i = 0; i < nfiles; i++) {
            const char *path     = files->filename[i];
            int         is_stdin = strcmp(path, "-") == 0;
            FILE       *fp       = is_stdin ? stdin : fopen(path, "r");

            if (!fp) {
                fprintf(stderr, "grep: %s: %s\n", path, strerror(errno));
                ret = 2;
                continue;
            }

            int m = grep_stream(fp, is_stdin ? NULL : path, &opts, i == nfiles - 1);
            if (m < 0) ret = 2;
            else if (m == 0 && ret == 0) ret = 1;
            else if (m > 0) ret = 0;
            total_match += m < 0 ? 0 : m;

            if (!is_stdin) fclose(fp);
        }
    }

    if (opts.use_json) printf("]\n");

    regfree(&re);
    arg_freetable(argtable, 10);

    /* grep exit code: 0=match found, 1=no match, 2=error */
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_grep_spec = {
    .name       = "grep",
    .summary    = "search for patterns in files",
    .long_help  = "Search for PATTERN (a POSIX extended regular expression) in each FILE. "
                  "Prints each matching line. Reads stdin when no files are given. "
                  "Exit status: 0 if match found, 1 if no match, 2 on error. "
                  "Use --json to emit structured JSON output.",
    .run         = grep_run,
    .print_usage = grep_print_usage,
};
