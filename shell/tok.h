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
 *   - 'single' quoting (fully literal) and "double" quoting
 *     ($ still expands; \" \\ \$ are escapes; all else literal)
 *   - operators < > >> | returned as their own words
 *   - # starts a comment (rest of line ignored)
 *
 * last_status is the most recent exit code, used to expand $?.
 *
 * Returns 0 on success, -1 on unmatched quote.
 * On success, caller must call tok_free() when done.
 */
int  tok_split(const char *line, tok_t *out, int last_status);
void tok_free(tok_t *t);

#endif /* TOK_H */
