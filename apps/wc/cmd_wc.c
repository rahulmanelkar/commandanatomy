#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_wc_argtable(
    struct arg_lit  **help,
    struct arg_lit  **lines,
    struct arg_lit  **words,
    struct arg_lit  **bytes,
    struct arg_lit  **chars,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void            **tbl)         /* caller-allocated array of 9 slots */
{
    *help  = arg_lit0("h", "help",  "show this help and exit");
    *lines = arg_lit0("l", "lines", "print newline count");
    *words = arg_lit0("w", "words", "print word count");
    *bytes = arg_lit0("c", "bytes", "print byte count");
    *chars = arg_lit0("m", "chars", "print character count");
    *json  = arg_lit0(NULL, "json", "emit machine-readable JSON (for agents/MCP)");
    *files = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to count (default: stdin)");
    *end   = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *lines;
    tbl[2] = *words;
    tbl[3] = *bytes;
    tbl[4] = *chars;
    tbl[5] = *json;
    tbl[6] = *files;
    tbl[7] = *end;
    tbl[8] = NULL;
}

/* --------------------------------------------------------------------------
 * counting
 * -------------------------------------------------------------------------- */

typedef struct {
    long long lines;
    long long words;
    long long bytes;
    long long chars;
} counts_t;

static counts_t count_stream(FILE *fp)
{
    counts_t c = {0, 0, 0, 0};
    int in_word = 0;
    int ch;

    while ((ch = fgetc(fp)) != EOF) {
        c.bytes++;
        /* For ASCII/UTF-8, count every non-continuation byte as a character. */
        if ((ch & 0xC0) != 0x80)
            c.chars++;
        if (ch == '\n')
            c.lines++;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            in_word = 0;
        } else {
            if (!in_word) { c.words++; in_word = 1; }
        }
    }
    return c;
}

/* --------------------------------------------------------------------------
 * output
 * -------------------------------------------------------------------------- */

static void print_plain_row(FILE *out, counts_t c, int show_l, int show_w, int show_b,
                             int show_m, const char *label)
{
    if (show_l) fprintf(out, " %7lld", c.lines);
    if (show_w) fprintf(out, " %7lld", c.words);
    if (show_b) fprintf(out, " %7lld", c.bytes);
    if (show_m) fprintf(out, " %7lld", c.chars);
    if (label)  fprintf(out, " %s", label);
    fprintf(out, "\n");
}

static void print_json_row(FILE *out, counts_t c, int show_l, int show_w, int show_b,
                            int show_m, const char *label, int trailing_comma)
{
    fprintf(out, "  {");
    if (label) fprintf(out, "\"file\": \"%s\", ", label);
    int first = 1;
#define FIELD(flag, key, val) \
    if (flag) { if (!first) fprintf(out, ", "); fprintf(out, "\"" key "\": %lld", val); first = 0; }
    FIELD(show_l, "lines", c.lines)
    FIELD(show_w, "words", c.words)
    FIELD(show_b, "bytes", c.bytes)
    FIELD(show_m, "chars", c.chars)
#undef FIELD
    fprintf(out, "}%s\n", trailing_comma ? "," : "");
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void wc_print_usage(FILE *out)
{
    struct arg_lit  *help, *lines, *words, *bytes, *chars, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void            *tbl[9];

    build_wc_argtable(&help, &lines, &words, &bytes, &chars, &json, &files, &end, tbl);

    fprintf(out, "Usage: wc ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nCount lines, words, and bytes in files.\n"
                 "With no mode flags, defaults to -lwc (lines, words, bytes).\n\n"
                 "Options:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 8);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int wc_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    struct arg_lit  *help, *lines, *words, *bytes, *chars, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void            *tbl[9];

    build_wc_argtable(&help, &lines, &words, &bytes, &chars, &json, &files, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        wc_print_usage(out_stream);
        arg_freetable(tbl, 8);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "wc");
        fprintf(stderr, "Try 'wc --help' for more information.\n");
        arg_freetable(tbl, 8);
        return 1;
    }

    /* Default to -lwc when no mode flag given. */
    int show_l = lines->count > 0;
    int show_w = words->count > 0;
    int show_b = bytes->count > 0;
    int show_m = chars->count > 0;
    int use_json = json->count > 0;

    if (!show_l && !show_w && !show_b && !show_m) {
        show_l = show_w = show_b = 1;
    }

    int nfiles = files->count;
    int ret = 0;

    if (use_json) fprintf(out_stream, "[\n");

    if (nfiles == 0) {
        counts_t c = count_stream(in_stream);
        if (use_json)
            print_json_row(out_stream, c, show_l, show_w, show_b, show_m, NULL, 0);
        else
            print_plain_row(out_stream, c, show_l, show_w, show_b, show_m, NULL);
    } else {
        counts_t total = {0, 0, 0, 0};

        for (int i = 0; i < nfiles; i++) {
            const char *path = files->filename[i];
            FILE *fp = fopen(path, "rb");
            if (!fp) {
                fprintf(stderr, "wc: %s: %s\n", path, strerror(errno));
                ret = 1;
                continue;
            }
            counts_t c = count_stream(fp);
            fclose(fp);

            total.lines += c.lines;
            total.words += c.words;
            total.bytes += c.bytes;
            total.chars += c.chars;

            int more = (nfiles > 1) && (i + 1 < nfiles);
            if (use_json)
                print_json_row(out_stream, c, show_l, show_w, show_b, show_m, path,
                               more || nfiles > 1);
            else
                print_plain_row(out_stream, c, show_l, show_w, show_b, show_m, path);
        }

        if (nfiles > 1) {
            if (use_json)
                print_json_row(out_stream, total, show_l, show_w, show_b, show_m, "total", 0);
            else
                print_plain_row(out_stream, total, show_l, show_w, show_b, show_m, "total");
        }
    }

    if (use_json) fprintf(out_stream, "]\n");

    arg_freetable(tbl, 8);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_wc_spec = {
    .name       = "wc",
    .summary    = "count lines, words, and bytes",
    .long_help  = "Count newlines, words, and bytes (or characters) in each FILE, "
                  "and print totals when multiple files are given. "
                  "Reads standard input when no files are specified. "
                  "Use --json for stable machine-readable output.",
    .run         = wc_run,
    .print_usage = wc_print_usage,
};
