#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_echo_argtable(
    struct arg_lit  **help,
    struct arg_lit  **no_newline,
    struct arg_lit  **escape,
    struct arg_lit  **json,
    struct arg_str  **words,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help       = arg_lit0("h", "help",       "show this help and exit");
    *no_newline = arg_lit0("n", NULL,          "do not output trailing newline");
    *escape     = arg_lit0("e", NULL,          "enable interpretation of backslash escapes");
    *json       = arg_lit0(NULL, "json",       "emit machine-readable JSON (for agents/MCP)");
    *words      = arg_strn(NULL, NULL, "[STRING...]", 0, 64, "strings to echo");
    *end        = arg_end(20);

    static void *tbl[7];
    tbl[0] = *help;
    tbl[1] = *no_newline;
    tbl[2] = *escape;
    tbl[3] = *json;
    tbl[4] = *words;
    tbl[5] = *end;
    tbl[6] = NULL;

    *argtable_out = tbl;
}

/* --------------------------------------------------------------------------
 * helpers
 * -------------------------------------------------------------------------- */

static void json_escape(FILE *out, const char *s)
{
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if      (ch == '"')  fputs("\\\"", out);
        else if (ch == '\\') fputs("\\\\", out);
        else if (ch == '\n') fputs("\\n",  out);
        else if (ch == '\r') fputs("\\r",  out);
        else if (ch == '\t') fputs("\\t",  out);
        else if (ch < 0x20)  fprintf(out, "\\u%04x", ch);
        else                 fputc(ch, out);
    }
}

/* Write a string interpreting \n, \t, \r, \\, \0, \a, \b, \f, \v escapes. */
static void print_escaped(const char *s)
{
    for (; *s; s++) {
        if (*s == '\\' && s[1]) {
            s++;
            switch (*s) {
            case 'n':  putchar('\n'); break;
            case 't':  putchar('\t'); break;
            case 'r':  putchar('\r'); break;
            case '\\': putchar('\\'); break;
            case '0':  putchar('\0'); break;
            case 'a':  putchar('\a'); break;
            case 'b':  putchar('\b'); break;
            case 'f':  putchar('\f'); break;
            case 'v':  putchar('\v'); break;
            default:   putchar('\\'); putchar(*s); break;
            }
        } else {
            putchar(*s);
        }
    }
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void echo_print_usage(FILE *out)
{
    struct arg_lit *help, *no_newline, *escape, *json;
    struct arg_str *words;
    struct arg_end *end;
    void          **argtable;

    build_echo_argtable(&help, &no_newline, &escape, &json, &words, &end, &argtable);

    fprintf(out, "Usage: echo ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nEcho the STRING(s) to standard output.\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");

    arg_freetable(argtable, 6);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int echo_run(int argc, char **argv)
{
    struct arg_lit *help, *no_newline, *escape, *json;
    struct arg_str *words;
    struct arg_end *end;
    void          **argtable;

    build_echo_argtable(&help, &no_newline, &escape, &json, &words, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        echo_print_usage(stdout);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "echo");
        fprintf(stderr, "Try 'echo --help' for more information.\n");
        arg_freetable(argtable, 6);
        return 1;
    }

    int use_no_newline = no_newline->count > 0;
    int use_escape     = escape->count     > 0;
    int use_json       = json->count       > 0;
    int nwords         = words->count;

    if (use_json) {
        printf("{\"output\": \"");
        for (int i = 0; i < nwords; i++) {
            if (i > 0) putchar(' ');
            json_escape(stdout, words->sval[i]);
        }
        if (!use_no_newline) fputs("\\n", stdout);
        printf("\"}\n");
    } else {
        for (int i = 0; i < nwords; i++) {
            if (i > 0) putchar(' ');
            if (use_escape)
                print_escaped(words->sval[i]);
            else
                fputs(words->sval[i], stdout);
        }
        if (!use_no_newline) putchar('\n');
    }

    arg_freetable(argtable, 6);
    return 0;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_echo_spec = {
    .name       = "echo",
    .summary    = "display a line of text",
    .long_help  = "Echo the STRING(s) to standard output. "
                  "Use -n to suppress the trailing newline. "
                  "Use -e to interpret backslash escapes (\\n, \\t, \\r, \\\\, etc.). "
                  "Use --json to emit structured JSON output.",
    .run         = echo_run,
    .print_usage = echo_print_usage,
};
