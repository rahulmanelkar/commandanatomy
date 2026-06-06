/*
 * linedit — a minimal raw-mode line editor for mysh's interactive prompt
 *
 * Canonical ("cooked") terminal input only allows editing at the end of the
 * line: the kernel buffers the line and backspace is the only editing key.
 * Arrow keys arrive as multi-byte escape sequences (e.g. ESC [ D for Left)
 * which the line discipline happily stores into the buffer as garbage.
 *
 * This module switches the terminal into raw mode for the duration of one
 * line read and implements the editing keys itself:
 *
 *   Left / Right         move the cursor one character
 *   Ctrl-Left / -Right   move the cursor one word
 *   Home / End           jump to start / end of line  (also Ctrl-A / Ctrl-E)
 *   Ctrl-B / Ctrl-F      move one character (same as the arrows)
 *   Backspace            delete the character before the cursor
 *   Delete               delete the character under the cursor
 *   Ctrl-U               kill from start of line to the cursor
 *   Ctrl-K               kill from the cursor to end of line
 *   Ctrl-W               kill the word before the cursor
 *   Ctrl-L               clear the screen, keep the current line
 *   Ctrl-C               abandon the line (prints "^C", returns empty)
 *   Ctrl-D               EOF at an empty prompt; Delete otherwise
 *   Enter                accept the line
 *   Up / Down            consumed and ignored (no history yet)
 *
 * Display strategy (borrowed from linenoise): the edit always occupies a
 * single terminal row.  If prompt + line exceed the terminal width, the
 * visible portion scrolls horizontally so the cursor stays on screen.
 * Every keystroke redraws the row:
 *
 *   \r  PROMPT  <visible window>  ESC[0K  \r ESC[<col>C
 *
 * Raw mode clears ECHO | ICANON | ISIG | IEXTEN, so Ctrl-C/Z/\ arrive as
 * plain bytes and are handled (or ignored) by the editor instead of raising
 * signals.  The original settings are restored with TCSADRAIN — pending
 * type-ahead (e.g. a pasted second command line) is preserved — before
 * linedit_read returns, so commands always run in the original mode.  An
 * atexit handler restores the terminal even if the shell exits mid-edit.
 *
 * Known limitation: cursor arithmetic counts bytes, so multi-byte UTF-8
 * input edits correctly but draws the cursor slightly off until the next
 * full redraw.  ASCII is exact.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "linedit.h"

/* Milliseconds to wait for the continuation bytes of an escape sequence.
 * A terminal sends a whole arrow-key sequence in one burst, so a short
 * window cleanly separates "pressed the Escape key" from "pressed Left". */
#define ESC_BYTE_TIMEOUT_MS 50

/* ── raw mode enter/leave ────────────────────────────────────────────────── */

static struct termios g_orig_termios;  /* saved settings, valid while g_raw */
static int            g_raw    = 0;    /* raw mode currently active         */
static int            g_raw_fd = -1;

static void raw_disable(void)
{
    if (g_raw) {
        /* TCSADRAIN: let queued output finish drawing, but keep any
         * type-ahead the user has already entered. */
        tcsetattr(g_raw_fd, TCSADRAIN, &g_orig_termios);
        g_raw = 0;
    }
}

static int raw_enable(int fd)
{
    if (tcgetattr(fd, &g_orig_termios) == -1) return -1;

    /* Restore the terminal even if the shell exits from inside the editor. */
    static int registered = 0;
    if (!registered) { atexit(raw_disable); registered = 1; }

    struct termios raw = g_orig_termios;
    raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;   /* read() blocks until at least one byte */
    raw.c_cc[VTIME] = 0;
    /* c_oflag stays untouched: "\n" -> CRLF post-processing keeps working. */

    if (tcsetattr(fd, TCSADRAIN, &raw) == -1) return -1;
    g_raw    = 1;
    g_raw_fd = fd;
    return 0;
}

/* ── low-level byte I/O ──────────────────────────────────────────────────── */

/* Read one byte, retrying on EINTR.  Returns 1, or 0 on EOF/error. */
static int read_byte(int fd, unsigned char *c)
{
    for (;;) {
        ssize_t n = read(fd, c, 1);
        if (n == 1) return 1;
        if (n == -1 && errno == EINTR) continue;
        return 0;
    }
}

/* Read the next byte only if it arrives within `ms` milliseconds.  Used for
 * the bytes following an ESC: a lone Escape keypress has no continuation,
 * an arrow key's full sequence arrives within a few milliseconds. */
static int read_byte_timeout(int fd, unsigned char *c, int ms)
{
    struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
    for (;;) {
        int r = poll(&pfd, 1, ms);
        if (r == -1 && errno == EINTR) continue;
        if (r <= 0) return 0;            /* timeout or error: no byte */
        return read_byte(fd, c);
    }
}

static void write_all(int fd, const char *buf, size_t n)
{
    while (n > 0) {
        ssize_t w = write(fd, buf, n);
        if (w == -1) {
            if (errno == EINTR) continue;
            return;                      /* tty gone; nothing useful to do */
        }
        buf += w;
        n   -= (size_t)w;
    }
}

static int term_cols(int ofd)
{
    struct winsize ws;
    if (ioctl(ofd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    return 80;                           /* pty with no size set, etc. */
}

/* ── editor state ────────────────────────────────────────────────────────── */

typedef struct {
    int         ifd, ofd;   /* tty file descriptors                          */
    char       *buf;        /* caller's line buffer                          */
    size_t      bufsz;      /* its capacity (including the NUL)              */
    size_t      len;        /* current line length                           */
    size_t      pos;        /* cursor index into buf, 0..len                 */
    const char *prompt;
    size_t      plen;       /* strlen(prompt)                                */
    char       *draw;       /* scratch buffer for one refresh frame          */
} le_t;

/*
 * Redraw the prompt row.  The line never wraps: if prompt + line exceed the
 * terminal width, leading characters scroll off so the cursor stays visible.
 */
static void le_refresh(le_t *l)
{
    int cols = term_cols(l->ofd);        /* re-query: tracks live resizes */
    int plen = (int)l->plen;
    int len  = (int)l->len;
    int pos  = (int)l->pos;
    const char *start = l->buf;

    /* Scroll: drop characters on the left until the cursor column fits... */
    while (pos > 0 && plen + pos >= cols) { start++; len--; pos--; }
    /* ...then clip on the right so the row never reaches the final column
     * (writing into the last column triggers terminal auto-wrap). */
    while (len > pos && plen + len >= cols) len--;

    char *d = l->draw;
    int   n = 0;

    d[n++] = '\r';                                     /* column 0          */
    memcpy(d + n, l->prompt, (size_t)plen); n += plen; /* prompt            */
    for (int i = 0; i < len; i++) {                    /* visible window;   */
        unsigned char c = (unsigned char)start[i];     /* control bytes     */
        d[n++] = (char)(c < 32 ? ' ' : c);             /* render as spaces  */
    }
    n += sprintf(d + n, "\x1b[0K");                    /* erase stale tail  */
    d[n++] = '\r';
    if (plen + pos > 0)
        n += sprintf(d + n, "\x1b[%dC", plen + pos);   /* park the cursor   */
    write_all(l->ofd, d, (size_t)n);
}

/* ── editing primitives (each leaves the screen refreshed) ───────────────── */

static void le_insert(le_t *l, unsigned char c)
{
    if (l->len + 1 >= l->bufsz) return;          /* line full: drop the key */
    memmove(l->buf + l->pos + 1, l->buf + l->pos, l->len - l->pos);
    l->buf[l->pos] = (char)c;
    l->pos++;
    l->len++;
    l->buf[l->len] = '\0';
    le_refresh(l);
}

static void le_delete_at(le_t *l)                /* Delete / Ctrl-D */
{
    if (l->pos >= l->len) return;
    memmove(l->buf + l->pos, l->buf + l->pos + 1, l->len - l->pos - 1);
    l->len--;
    l->buf[l->len] = '\0';
    le_refresh(l);
}

static void le_backspace(le_t *l)
{
    if (l->pos == 0) return;
    memmove(l->buf + l->pos - 1, l->buf + l->pos, l->len - l->pos);
    l->pos--;
    l->len--;
    l->buf[l->len] = '\0';
    le_refresh(l);
}

static void le_move(le_t *l, size_t pos)
{
    l->pos = pos;
    le_refresh(l);
}

static void le_kill_to_end(le_t *l)              /* Ctrl-K */
{
    l->len = l->pos;
    l->buf[l->len] = '\0';
    le_refresh(l);
}

static void le_kill_to_start(le_t *l)            /* Ctrl-U */
{
    memmove(l->buf, l->buf + l->pos, l->len - l->pos);
    l->len -= l->pos;
    l->pos  = 0;
    l->buf[l->len] = '\0';
    le_refresh(l);
}

/* Word boundaries: blanks separate words (matches the tokenizer's view). */
static size_t word_left(const le_t *l, size_t pos)
{
    while (pos > 0 && isblank((unsigned char)l->buf[pos - 1]))  pos--;
    while (pos > 0 && !isblank((unsigned char)l->buf[pos - 1])) pos--;
    return pos;
}

static size_t word_right(const le_t *l, size_t pos)
{
    while (pos < l->len && isblank((unsigned char)l->buf[pos]))  pos++;
    while (pos < l->len && !isblank((unsigned char)l->buf[pos])) pos++;
    return pos;
}

static void le_kill_prev_word(le_t *l)           /* Ctrl-W */
{
    size_t from = word_left(l, l->pos);
    memmove(l->buf + from, l->buf + l->pos, l->len - l->pos);
    l->len -= l->pos - from;
    l->pos  = from;
    l->buf[l->len] = '\0';
    le_refresh(l);
}

/* ── escape sequence decoding ────────────────────────────────────────────── */

/*
 * Called after a raw ESC (0x1b) byte.  Decodes the common CSI ("ESC [ ...")
 * and SS3 ("ESC O ...") key encodings:
 *
 *   ESC [ C / D           Right / Left
 *   ESC [ 1 ; 5 C / D     Ctrl-Right / Ctrl-Left   (word movement)
 *   ESC [ H / F           Home / End
 *   ESC [ 1~ 7~ / 4~ 8~   Home / End               (vt / rxvt variants)
 *   ESC [ 3 ~             Delete
 *   ESC O A-D / H / F     application-mode arrows, Home, End
 *
 * Everything else — including Up/Down (no history yet), PgUp/PgDn, F-keys —
 * is consumed silently so unknown keys never leak bytes into the line.  A
 * lone Escape keypress has no continuation bytes and is dropped via the
 * read timeout.
 *
 * Returns -1 when the sequence was fully consumed.  If the sequence turns
 * out to be malformed — a byte that cannot belong to it (e.g. Enter,
 * Ctrl-C, a pasted character) shows up where a sequence byte belongs —
 * that byte is *returned* so the caller can dispatch it as a keypress
 * instead of losing it.
 */
static int le_escape(le_t *l)
{
    unsigned char b;

    if (!read_byte_timeout(l->ifd, &b, ESC_BYTE_TIMEOUT_MS)) return -1;
    if (b < 32 || b == 127) return b;        /* ESC then a control key */

    if (b == '[') {                                    /* CSI sequences */
        if (!read_byte_timeout(l->ifd, &b, ESC_BYTE_TIMEOUT_MS)) return -1;

        /* CSI syntax (ECMA-48): any parameter bytes 0x30-0x3F and
         * intermediate bytes 0x20-0x2F, then one final byte 0x40-0x7E.
         * Track up to two numeric parameters ("1;5C" -> p1=1, p2=5);
         * everything else is consumed untracked. */
        int p1 = 0, p2 = 0, *cur = &p1;
        while (b >= 0x20 && b <= 0x3F) {
            if (b >= '0' && b <= '9') *cur = *cur * 10 + (b - '0');
            else if (b == ';')        { cur = &p2; *cur = 0; }
            if (!read_byte_timeout(l->ifd, &b, ESC_BYTE_TIMEOUT_MS)) return -1;
        }
        if (b < 0x40 || b > 0x7E) return b;            /* malformed sequence */

        switch (b) {
        case 'C':                                      /* Right */
            if (p2 == 5)              le_move(l, word_right(l, l->pos));
            else if (l->pos < l->len) le_move(l, l->pos + 1);
            break;
        case 'D':                                      /* Left */
            if (p2 == 5)              le_move(l, word_left(l, l->pos));
            else if (l->pos > 0)      le_move(l, l->pos - 1);
            break;
        case 'H': le_move(l, 0);      break;           /* Home */
        case 'F': le_move(l, l->len); break;           /* End  */
        case '~':                                      /* vt-style keys */
            if      (p1 == 1 || p1 == 7) le_move(l, 0);
            else if (p1 == 4 || p1 == 8) le_move(l, l->len);
            else if (p1 == 3)            le_delete_at(l);
            break;                       /* 2~ Insert, 5~/6~ PgUp/PgDn: ignored */
        default:  break;                 /* A/B (history) etc.: ignored */
        }

    } else if (b == 'O') {                             /* SS3 sequences */
        if (!read_byte_timeout(l->ifd, &b, ESC_BYTE_TIMEOUT_MS)) return -1;
        if (b < 32 || b == 127) return b;    /* truncated sequence */
        switch (b) {
        case 'C': if (l->pos < l->len) le_move(l, l->pos + 1); break;
        case 'D': if (l->pos > 0)      le_move(l, l->pos - 1); break;
        case 'H': le_move(l, 0);      break;
        case 'F': le_move(l, l->len); break;
        default:  break;
        }
    }
    /* Other ESC-prefixed input (Alt-<key> chords, ...): ignored. */
    return -1;
}

/* ── the editor proper ───────────────────────────────────────────────────── */

/*
 * Returns:  >= 0  line accepted (its length; buf is NUL-terminated)
 *           -1    end of input (Ctrl-D at an empty prompt, or tty hangup)
 */
static int le_edit(le_t *l)
{
    l->len = l->pos = 0;
    l->buf[0] = '\0';
    le_refresh(l);                       /* draw the prompt */

    for (;;) {
        unsigned char c;

        if (!read_byte(l->ifd, &c)) return -1;

redispatch:
        switch (c) {
        case '\r':                       /* Enter (ICRNL is off) */
        case '\n':
            return (int)l->len;

        case 3:                          /* Ctrl-C: abandon the line */
            le_move(l, l->len);          /* park the cursor at the end...  */
            write_all(l->ofd, "^C", 2);  /* ...so ^C lands after the text  */
            l->len = l->pos = 0;
            l->buf[0] = '\0';
            return 0;                    /* empty line: fresh prompt */

        case 4:                          /* Ctrl-D */
            if (l->len == 0) return -1;  /* EOF at an empty prompt */
            le_delete_at(l);
            break;

        case 127:                        /* Backspace (DEL) */
        case 8:                          /* Ctrl-H */
            le_backspace(l);
            break;

        case 27: {                       /* ESC: arrow keys & friends */
            int leftover = le_escape(l);
            if (leftover >= 0) {         /* malformed sequence ended in a
                                          * byte that isn't part of it —
                                          * don't lose that keypress      */
                c = (unsigned char)leftover;
                goto redispatch;
            }
            break;
        }

        case 1:  le_move(l, 0);      break;                     /* Ctrl-A */
        case 5:  le_move(l, l->len); break;                     /* Ctrl-E */
        case 2:  if (l->pos > 0)      le_move(l, l->pos - 1); break; /* ^B */
        case 6:  if (l->pos < l->len) le_move(l, l->pos + 1); break; /* ^F */
        case 11: le_kill_to_end(l);    break;                   /* Ctrl-K */
        case 21: le_kill_to_start(l);  break;                   /* Ctrl-U */
        case 23: le_kill_prev_word(l); break;                   /* Ctrl-W */

        case 12:                         /* Ctrl-L: clear screen, redraw */
            write_all(l->ofd, "\x1b[H\x1b[2J", 7);
            le_refresh(l);
            break;

        case '\t':                       /* no completion: keep a literal tab
                                          * (refresh renders it as a space) */
            le_insert(l, c);
            break;

        default:
            /* Printable ASCII and UTF-8 bytes are inserted; remaining
             * control bytes are ignored rather than corrupting the line. */
            if (c >= 32) le_insert(l, c);
            break;
        }
    }
}

/* ── public entry point ──────────────────────────────────────────────────── */

/* Terminals that cannot interpret cursor-movement escapes. */
static int term_is_dumb(void)
{
    const char *term = getenv("TERM");
    if (!term) return 0;                 /* unset: assume a real terminal */
    return strcmp(term, "dumb")   == 0 ||
           strcmp(term, "cons25") == 0 ||
           strcmp(term, "emacs")  == 0;
}

int linedit_read(FILE *in, const char *prompt, char *buf, size_t bufsz)
{
    if (bufsz == 0) return -1;

    int ifd = fileno(in);

    /* Raw-mode editing needs a capable tty on both ends: input for keys,
     * stdout for redrawing.  (With stdout redirected, cooked mode is the
     * better experience — the kernel still echoes what you type.) */
    if (isatty(ifd) && isatty(STDOUT_FILENO) && !term_is_dumb()) {
        char *draw = malloc(strlen(prompt) + bufsz + 48);
        if (draw && raw_enable(ifd) == 0) {
            /* Anything buffered (e.g. a previous command's output) must hit
             * the screen before we start drawing with raw write()s. */
            fflush(stdout);

            le_t l = {
                .ifd = ifd, .ofd = STDOUT_FILENO,
                .buf = buf, .bufsz = bufsz, .len = 0, .pos = 0,
                .prompt = prompt, .plen = strlen(prompt),
                .draw = draw,
            };
            int rc = le_edit(&l);

            raw_disable();
            free(draw);

            if (rc < 0) return -1;                    /* EOF */
            write_all(STDOUT_FILENO, "\n", 1);        /* echo of Enter */
            return 0;
        }
        free(draw);
        /* fall through to cooked mode */
    }

    /* Cooked-mode fallback: scripts, pipes, dumb terminals.  Identical to
     * the shell's historical prompt + fgets behaviour. */
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(buf, (int)bufsz, in)) return -1;
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}
