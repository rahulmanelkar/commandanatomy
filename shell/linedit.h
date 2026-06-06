#ifndef LINEDIT_H
#define LINEDIT_H

#include <stdio.h>
#include <stddef.h>

/*
 * Read one line of input with interactive editing.
 *
 * Draws `prompt`, then lets the user edit the line in place: Left/Right
 * arrows, Home/End, Delete, Backspace, kill shortcuts (see linedit.c for
 * the full key map).  The terminal is in raw mode only for the duration
 * of the call; commands always run with the terminal restored.
 *
 * If `in` (or stdout) is not a capable terminal — script file, pipe,
 * TERM=dumb — falls back to printing the prompt and reading one line with
 * fgets, so non-interactive input behaves exactly like stock stdio.
 *
 * On success the line is stored NUL-terminated in buf (without the
 * trailing newline) and 0 is returned.  Returns -1 on end of input
 * (Ctrl-D at an empty prompt, or EOF).
 */
int linedit_read(FILE *in, const char *prompt, char *buf, size_t bufsz);

#endif /* LINEDIT_H */
