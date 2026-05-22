#ifndef TOK_H
#define TOK_H

#define TOK_MAX 128

typedef struct {
    char *w[TOK_MAX];   /* heap-allocated word strings, null-terminated list */
    int   n;
} tok_t;

/*
 * Split line into tokens.  Recognises:
 *   - whitespace word boundaries
 *   - 'single' and "double" quoting (no variable expansion)
 *   - operators < > >> | returned as their own words
 *   - # starts a comment (rest of line ignored)
 *
 * Returns 0 on success, -1 on unmatched quote.
 * On success, caller must call tok_free() when done.
 */
int  tok_split(const char *line, tok_t *out);
void tok_free(tok_t *t);

#endif /* TOK_H */
