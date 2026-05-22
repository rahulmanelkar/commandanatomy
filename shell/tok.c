#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tok.h"

#define WORDBUF 4096

static int push_word(tok_t *t, const char *buf, int len)
{
    if (t->n >= TOK_MAX) {
        fprintf(stderr, "mysh: too many tokens on one line\n");
        return -1;
    }
    char *s = malloc((size_t)len + 1);
    if (!s) { perror("malloc"); return -1; }
    memcpy(s, buf, (size_t)len);
    s[len] = '\0';
    t->w[t->n++] = s;
    return 0;
}

int tok_split(const char *line, tok_t *out)
{
    out->n = 0;

    char buf[WORDBUF];
    int  blen   = 0;
    int  in_sq  = 0;   /* inside '...' */
    int  in_dq  = 0;   /* inside "..." */
    int  in_word = 0;  /* building a word (matters for empty-quote handling) */

    for (const char *p = line; ; p++) {
        char c = *p;

        /* ── single-quote mode ─────────────────────────────────────────── */
        if (in_sq) {
            if (c == '\0') {
                fprintf(stderr, "mysh: unmatched single quote\n");
                tok_free(out);
                return -1;
            }
            if (c == '\'') { in_sq = 0; continue; }
            if (blen < WORDBUF - 1) buf[blen++] = c;
            continue;
        }

        /* ── double-quote mode ─────────────────────────────────────────── */
        if (in_dq) {
            if (c == '\0') {
                fprintf(stderr, "mysh: unmatched double quote\n");
                tok_free(out);
                return -1;
            }
            if (c == '"')  { in_dq = 0; continue; }
            if (c == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$')) {
                p++;
                c = *p;
            }
            if (blen < WORDBUF - 1) buf[blen++] = c;
            continue;
        }

        /* ── normal mode ───────────────────────────────────────────────── */

        if (c == '\0' || isspace((unsigned char)c)) {
            /* end of word */
            if (in_word || blen > 0) {
                if (push_word(out, buf, blen) < 0) { tok_free(out); return -1; }
                blen = 0;
                in_word = 0;
            }
            if (c == '\0') break;
            continue;
        }

        if (c == '#' && !in_word) break;   /* comment */

        if (c == '\'') { in_sq = 1; in_word = 1; continue; }
        if (c == '"')  { in_dq = 1; in_word = 1; continue; }

        if (c == '>' || c == '<' || c == '|') {
            /* flush any pending word first */
            if (in_word || blen > 0) {
                if (push_word(out, buf, blen) < 0) { tok_free(out); return -1; }
                blen = 0;
                in_word = 0;
            }
            if (c == '>' && p[1] == '>') {
                if (push_word(out, ">>", 2) < 0) { tok_free(out); return -1; }
                p++;
            } else {
                if (push_word(out, (char[]){c, '\0'}, 1) < 0) { tok_free(out); return -1; }
            }
            continue;
        }

        /* ordinary character */
        if (blen < WORDBUF - 1) buf[blen++] = c;
        in_word = 1;
    }

    return 0;
}

void tok_free(tok_t *t)
{
    for (int i = 0; i < t->n; i++) { free(t->w[i]); t->w[i] = NULL; }
    t->n = 0;
}
