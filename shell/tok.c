#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tok.h"

#define WORDBUF 4096

/* Expand $VAR / ${VAR} / $? at position *pp (which points to '$').
 * Appends the expanded text into buf[0..WORDBUF-1] via *blen.
 * On return *pp points to the last consumed character so the caller's
 * loop p++ lands on the first unprocessed character. */
static void do_expand(const char **pp, char *buf, int *blen, int last_status)
{
    const char *p = *pp;
    p++;  /* skip '$' */

    char varname[256];
    int  vlen = 0;
    char numbuf[32];
    const char *val = NULL;

    if (*p == '?') {
        snprintf(numbuf, sizeof numbuf, "%d", last_status);
        val = numbuf;
        /* p stays at '?'; loop p++ will skip it */
    } else if (*p == '{') {
        p++;  /* skip '{' */
        while (*p && *p != '}') {
            if (vlen < 255) varname[vlen++] = *p;
            p++;
        }
        varname[vlen] = '\0';
        if (*p == '\0') p--;  /* don't advance past NUL */
        /* p at '}'; loop p++ skips it */
        val = getenv(varname);
    } else if (isalpha((unsigned char)*p) || *p == '_') {
        while (isalnum((unsigned char)*p) || *p == '_') {
            if (vlen < 255) varname[vlen++] = *p;
            p++;
        }
        varname[vlen] = '\0';
        p--;  /* loop p++ will advance past last identifier char */
        val = getenv(varname);
    } else {
        /* bare '$': emit literally, back up so current char is re-examined */
        if (*blen < WORDBUF - 1) buf[(*blen)++] = '$';
        p--;
    }

    if (val) {
        for (const char *v = val; *v && *blen < WORDBUF - 1; v++)
            buf[(*blen)++] = *v;
    }

    *pp = p;
}

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

int tok_split(const char *line, tok_t *out, int last_status)
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
                /* fall through: write escaped char literally */
            } else if (c == '$') {
                do_expand(&p, buf, &blen, last_status);
                continue;
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

        if (c == '$') {
            do_expand(&p, buf, &blen, last_status);
            in_word = 1;
            continue;
        }

        if (c == '>' || c == '<' || c == '|' || c == '&') {
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
