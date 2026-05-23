#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_uniq_argtable(
    struct arg_lit  **help,
    struct arg_lit  **count,
    struct arg_lit  **repeated,
    struct arg_lit  **unique,
    struct arg_lit  **json,
    struct arg_file **infile,
    struct arg_file **outfile,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help     = arg_lit0("h", "help",           "show this help and exit");
    *count    = arg_lit0("c", "count",          "prefix lines with number of occurrences");
    *repeated = arg_lit0("d", "repeated",       "only print lines that occur more than once");
    *unique   = arg_lit0("u", "unique",         "only print lines that occur exactly once");
    *json     = arg_lit0(NULL, "json",          "emit machine-readable JSON (for agents/MCP)");
    *infile   = arg_file0(NULL, NULL, "[INPUT]",  "input file (default: stdin)");
    *outfile  = arg_file0(NULL, NULL, "[OUTPUT]", "output file (default: stdout)");
    *end      = arg_end(20);

    static void *tbl[9];
    tbl[0] = *help;
    tbl[1] = *count;
    tbl[2] = *repeated;
    tbl[3] = *unique;
    tbl[4] = *json;
    tbl[5] = *infile;
    tbl[6] = *outfile;
    tbl[7] = *end;
    tbl[8] = NULL;

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
    char *text;    /* line text including newline */
    size_t len;
    int    hits;   /* occurrence count */
} entry_t;

/* Collect (line, count) pairs from fp into a dynamic array. */
static int collect_entries(FILE *fp, entry_t **out, int *nout)
{
    int      cap  = 64;
    int      n    = 0;
    entry_t *ents = malloc(cap * sizeof(entry_t));
    if (!ents) return -1;

    char   *buf   = NULL;
    size_t  bufsz = 0;
    ssize_t len;

    while ((len = getline(&buf, &bufsz, fp)) != -1) {
        /* Compare with previous entry (adjacent duplicates). */
        if (n > 0 && strlen(ents[n - 1].text) == (size_t)len &&
            memcmp(ents[n - 1].text, buf, len) == 0) {
            ents[n - 1].hits++;
            continue;
        }

        if (n == cap) {
            cap *= 2;
            entry_t *tmp = realloc(ents, cap * sizeof(entry_t));
            if (!tmp) { free(buf); free(ents); return -1; }
            ents = tmp;
        }
        ents[n].text = strndup(buf, len);
        ents[n].len  = len;
        ents[n].hits = 1;
        n++;
    }

    free(buf);
    *out  = ents;
    *nout = n;
    return ferror(fp) ? -1 : 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void uniq_print_usage(FILE *out)
{
    struct arg_lit  *help, *count, *repeated, *unique, *json;
    struct arg_file *infile, *outfile;
    struct arg_end  *end;
    void           **argtable;

    build_uniq_argtable(&help, &count, &repeated, &unique, &json,
                        &infile, &outfile, &end, &argtable);

    fprintf(out, "Usage: uniq ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nFilter adjacent matching lines from INPUT (or stdin).\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");

    arg_freetable(argtable, 8);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int uniq_run(int argc, char **argv)
{
    struct arg_lit  *help, *count, *repeated, *unique, *json;
    struct arg_file *infile, *outfile;
    struct arg_end  *end;
    void           **argtable;

    build_uniq_argtable(&help, &count, &repeated, &unique, &json,
                        &infile, &outfile, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        uniq_print_usage(stdout);
        arg_freetable(argtable, 8);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "uniq");
        fprintf(stderr, "Try 'uniq --help' for more information.\n");
        arg_freetable(argtable, 8);
        return 1;
    }

    int use_count    = count->count    > 0;
    int use_repeated = repeated->count > 0;
    int use_unique   = unique->count   > 0;
    int use_json     = json->count     > 0;

    FILE *fp  = stdin;
    FILE *out = stdout;

    if (infile->count > 0) {
        fp = fopen(infile->filename[0], "r");
        if (!fp) {
            fprintf(stderr, "uniq: %s: %s\n", infile->filename[0], strerror(errno));
            arg_freetable(argtable, 8);
            return 1;
        }
    }
    if (outfile->count > 0) {
        out = fopen(outfile->filename[0], "w");
        if (!out) {
            fprintf(stderr, "uniq: %s: %s\n", outfile->filename[0], strerror(errno));
            if (fp != stdin) fclose(fp);
            arg_freetable(argtable, 8);
            return 1;
        }
    }

    entry_t *ents = NULL;
    int      n    = 0;
    int      rc   = collect_entries(fp, &ents, &n);

    if (fp != stdin)  fclose(fp);

    if (rc < 0) {
        fprintf(stderr, "uniq: read error or out of memory\n");
        if (out != stdout) fclose(out);
        arg_freetable(argtable, 8);
        return 1;
    }

    /* Determine which entries to emit. */
    /* Count non-filtered entries for JSON comma logic. */
    int emit_count = 0;
    for (int i = 0; i < n; i++) {
        if (use_repeated && ents[i].hits == 1) continue;
        if (use_unique   && ents[i].hits  > 1) continue;
        emit_count++;
    }

    if (use_json) fprintf(out, "[\n");

    int emitted = 0;
    for (int i = 0; i < n; i++) {
        if (use_repeated && ents[i].hits == 1) continue;
        if (use_unique   && ents[i].hits  > 1) continue;

        size_t slen = ents[i].len;
        if (slen > 0 && ents[i].text[slen - 1] == '\n') slen--;

        emitted++;
        if (use_json) {
            fprintf(out, "  {\"count\": %d, \"line\": \"", ents[i].hits);
            json_escape(out, ents[i].text, slen);
            fprintf(out, "\"}%s\n", emitted == emit_count ? "" : ",");
        } else {
            if (use_count)
                fprintf(out, "%7d ", ents[i].hits);
            fwrite(ents[i].text, 1, ents[i].len, out);
        }
    }

    if (use_json) fprintf(out, "]\n");

    for (int i = 0; i < n; i++) free(ents[i].text);
    free(ents);

    if (out != stdout) fclose(out);
    arg_freetable(argtable, 8);
    return 0;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_uniq_spec = {
    .name       = "uniq",
    .summary    = "report or omit repeated adjacent lines",
    .long_help  = "Filter adjacent matching lines from INPUT (default stdin), "
                  "writing to OUTPUT (default stdout). "
                  "Use -c to prefix lines with their occurrence count, "
                  "-d to show only duplicates, -u to show only unique lines. "
                  "Pair with sort to deduplicate non-adjacent matches. "
                  "Use --json to emit structured JSON output.",
    .run         = uniq_run,
    .print_usage = uniq_print_usage,
};
