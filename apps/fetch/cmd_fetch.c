#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>      /* struct timeval for SO_RCVTIMEO */

/* Bounds for input validation and network timeouts. */
#define FETCH_CONNECT_TIMEOUT_SECS 5
#define FETCH_RECV_TIMEOUT_SECS    5
#define FETCH_MAX_HOST_LEN         255   /* RFC 1035 hostname limit */
#define FETCH_PORT_MIN             1
#define FETCH_PORT_MAX             65535
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder — single source of truth for parsing and help output.
 *
 * Stack-allocated builder pattern: every argtable handle lives in the
 * caller's frame and is passed in by address, so build_fetch_argtable() owns
 * no static/global state.  Two threads may each hold their own set of handles
 * and call run()/print_usage() concurrently without interfering.
 * -------------------------------------------------------------------------- */

static void build_fetch_argtable(
    struct arg_lit  **help,
    struct arg_str  **host,
    struct arg_int  **port,
    struct arg_lit  **json,
    struct arg_str  **message,
    struct arg_end  **end,
    void            **tbl)         /* caller-allocated array of 7 slots */
{
    *help    = arg_lit0("h", "help",        "show this help and exit");
    *host    = arg_str1("H", "host", "HOST", "host to connect to");
    *port    = arg_int1("p", "port", "PORT", "TCP port to connect to");
    *json    = arg_lit0(NULL, "json",       "emit machine-readable JSON (for agents/MCP)");
    *message = arg_str1(NULL, NULL, "MESSAGE", "message to send to the host");
    *end     = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *host;
    tbl[2] = *port;
    tbl[3] = *json;
    tbl[4] = *message;
    tbl[5] = *end;
    tbl[6] = NULL;
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

/* Resolve host:port into an addrinfo list (caller frees with freeaddrinfo).
 * Returns 0 on success, -1 on failure with *errmsg set. */
static int resolve_host(const char *host, int port,
                        struct addrinfo **res, const char **errmsg)
{
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;   /* TCP */

    int rc = getaddrinfo(host, portstr, &hints, res);
    if (rc != 0) {
        *errmsg = gai_strerror(rc);
        return -1;
    }
    return 0;
}

/* connect() with a bounded timeout so an unreachable-but-routable host cannot
 * stall for the kernel's default connect timeout.  The socket is switched to
 * non-blocking for the attempt, the in-progress connect is waited on with
 * poll(), and the original blocking mode is restored on success.  Returns 0 on
 * success, -1 on error/timeout with errno set (ETIMEDOUT on timeout). */
static int connect_timeout(int fd, const struct sockaddr *addr,
                           socklen_t alen, int timeout_secs)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    int rc = connect(fd, addr, alen);
    if (rc == 0) {
        /* Connected immediately (e.g. loopback). */
        fcntl(fd, F_SETFL, flags);
        return 0;
    }
    if (errno != EINPROGRESS)
        return -1;                       /* hard failure, errno already set */

    /* Wait for the socket to become writable (connect complete) or time out. */
    struct pollfd pfd = { .fd = fd, .events = POLLOUT };
    int pr;
    do {
        pr = poll(&pfd, 1, timeout_secs * 1000);
    } while (pr < 0 && errno == EINTR);

    if (pr == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (pr < 0)
        return -1;

    /* poll() reports writability even on failed connects; check SO_ERROR. */
    int soerr = 0;
    socklen_t len = sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0)
        return -1;
    if (soerr != 0) {
        errno = soerr;
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags) < 0)   /* restore blocking mode */
        return -1;
    return 0;
}

/* send() the whole buffer, retrying short writes and EINTR.
 * Returns 0 on success, -1 on error. */
static int send_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void fetch_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_str  *host;
    struct arg_int  *port;
    struct arg_lit  *json;
    struct arg_str  *message;
    struct arg_end  *end;
    void            *tbl[7];

    build_fetch_argtable(&help, &host, &port, &json, &message, &end, tbl);

    fprintf(out, "Usage: fetch ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nConnect to HOST:PORT over TCP, send a newline-terminated "
                 "MESSAGE,\nand print the server's reply line.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 6);
}

/* --------------------------------------------------------------------------
 * run
 *
 * Implements the standard TCP client sequence — socket(), connect(), send(),
 * recv() — over a line-based protocol: the request is MESSAGE terminated by a
 * newline, and the reply is read up to (and including) its terminating
 * newline.  All human-facing output is written exclusively to out_stream so
 * the command stays isolated from the process's real stdout/stderr; in_stream
 * is the shell-supplied input channel (unused here, as the payload arrives as
 * an argument rather than on stdin).
 * -------------------------------------------------------------------------- */

int fetch_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    (void)in_stream;   /* payload comes from MESSAGE, not the input stream */

    struct arg_lit  *help;
    struct arg_str  *host;
    struct arg_int  *port;
    struct arg_lit  *json;
    struct arg_str  *message;
    struct arg_end  *end;
    void            *tbl[7];

    build_fetch_argtable(&help, &host, &port, &json, &message, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        fetch_print_usage(out_stream);
        arg_freetable(tbl, 6);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "fetch");
        fprintf(stderr, "Try 'fetch --help' for more information.\n");
        arg_freetable(tbl, 6);
        return 1;
    }

    const char *the_host = host->sval[0];
    int         the_port = port->ival[0];
    const char *the_msg  = message->sval[0];
    int         use_json = (json->count > 0);

    /* Report an error: structured JSON on out_stream when --json was given,
     * otherwise a clear human-readable message on stderr. */
    #define FETCH_FAIL(field, detail) do {                                    \
        if (use_json) {                                                       \
            fprintf(out_stream, "{\"ok\": false, \"host\": \"");              \
            json_escape(out_stream, the_host, strlen(the_host));             \
            fprintf(out_stream, "\", \"port\": %d, \"error\": \"", the_port); \
            json_escape(out_stream, (detail), strlen(detail));               \
            fprintf(out_stream, "\"}\n");                                     \
        } else {                                                              \
            fprintf(stderr, "fetch: %s: %s\n", (field), (detail));            \
        }                                                                     \
    } while (0)

    /* --- validate inputs are within safe bounds -------------------------- */
    size_t hostlen = strlen(the_host);
    if (hostlen == 0) {
        FETCH_FAIL("host", "host must not be empty");
        arg_freetable(tbl, 6);
        return 1;
    }
    if (hostlen > FETCH_MAX_HOST_LEN) {
        FETCH_FAIL("host", "host name too long");
        arg_freetable(tbl, 6);
        return 1;
    }
    for (size_t i = 0; i < hostlen; i++) {
        unsigned char ch = (unsigned char)the_host[i];
        if (ch < 0x20 || ch == 0x7f) {       /* reject control characters */
            FETCH_FAIL("host", "host contains invalid characters");
            arg_freetable(tbl, 6);
            return 1;
        }
    }

    if (the_port < FETCH_PORT_MIN || the_port > FETCH_PORT_MAX) {
        FETCH_FAIL("port", "port out of range (must be 1-65535)");
        arg_freetable(tbl, 6);
        return 1;
    }

    /* --- resolve ---------------------------------------------------------- */
    struct addrinfo *res = NULL;
    const char      *errmsg = NULL;
    if (resolve_host(the_host, the_port, &res, &errmsg) != 0) {
        FETCH_FAIL("resolve", errmsg);
        arg_freetable(tbl, 6);
        return 1;
    }

    /* --- socket() + connect() -------------------------------------------- */
    int fd = -1;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;                            /* try next candidate */
        if (connect_timeout(fd, rp->ai_addr, rp->ai_addrlen,
                            FETCH_CONNECT_TIMEOUT_SECS) == 0)
            break;                               /* connected */
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        FETCH_FAIL("connect", strerror(errno));
        arg_freetable(tbl, 6);
        return 1;
    }

    /* --- setsockopt(SO_RCVTIMEO) -----------------------------------------
     * Bound recv() so a silent or slow peer cannot hang the command: after
     * FETCH_RECV_TIMEOUT_SECS with no data, recv() fails with EAGAIN. */
    struct timeval tv;
    tv.tv_sec  = FETCH_RECV_TIMEOUT_SECS;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
        FETCH_FAIL("setsockopt", strerror(errno));
        close(fd);
        arg_freetable(tbl, 6);
        return 1;
    }

    /* --- send() ----------------------------------------------------------
     * Line-based protocol: every request ends with a newline.  We build the
     * framed request once and send it with retry-safe send_all(). */
    size_t  msglen   = strlen(the_msg);
    size_t  framelen = msglen + 1;               /* + '\n' */
    char   *frame    = malloc(framelen);
    if (!frame) {
        FETCH_FAIL("send", "out of memory");
        close(fd);
        arg_freetable(tbl, 6);
        return 1;
    }
    memcpy(frame, the_msg, msglen);
    frame[msglen] = '\n';

    if (send_all(fd, frame, framelen) != 0) {
        FETCH_FAIL("send", strerror(errno));
        free(frame);
        close(fd);
        arg_freetable(tbl, 6);
        return 1;
    }
    free(frame);

    /* --- recv() ----------------------------------------------------------
     * Read until the reply's terminating newline (line-based) or until the
     * peer closes the connection, accumulating into a growable buffer. */
    char  *reply = NULL;
    size_t cap   = 0;
    size_t total = 0;
    int    saw_newline = 0;
    char   buf[4096];
    ssize_t n;

    while (!saw_newline) {
        n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                FETCH_FAIL("recv", "timed out waiting for reply");
            } else {
                FETCH_FAIL("recv", strerror(errno));
            }
            free(reply);
            close(fd);
            arg_freetable(tbl, 6);
            return 1;
        }
        if (n == 0)
            break;                               /* peer closed */

        if (total + (size_t)n > cap) {
            cap = cap ? cap * 2 : 8192;
            while (cap < total + (size_t)n) cap *= 2;
            char *grown = realloc(reply, cap);
            if (!grown) {
                FETCH_FAIL("recv", "out of memory");
                free(reply);
                close(fd);
                arg_freetable(tbl, 6);
                return 1;
            }
            reply = grown;
        }
        memcpy(reply + total, buf, (size_t)n);
        total += (size_t)n;

        if (memchr(buf, '\n', (size_t)n) != NULL)
            saw_newline = 1;                     /* full line received */
    }
    close(fd);

    /* --- emit reply on out_stream ---------------------------------------- */
    if (use_json) {
        fprintf(out_stream, "{\"ok\": true, \"host\": \"");
        json_escape(out_stream, the_host, strlen(the_host));
        fprintf(out_stream, "\", \"port\": %d, \"sent\": \"", the_port);
        json_escape(out_stream, the_msg, msglen);
        fprintf(out_stream, "\", \"bytes\": %zu, \"reply\": \"", total);
        if (reply) json_escape(out_stream, reply, total);
        fprintf(out_stream, "\"}\n");
    } else {
        if (reply) fwrite(reply, 1, total, out_stream);
        if (total == 0 || reply[total - 1] != '\n') fputc('\n', out_stream);
    }

    free(reply);
    arg_freetable(tbl, 6);
    return 0;

    #undef FETCH_FAIL
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_fetch_spec = {
    .name       = "fetch",
    .summary    = "send a message to a host over TCP and print the reply",
    .long_help  = "Open a TCP connection to HOST:PORT, send MESSAGE followed "
                  "by a newline, then read and print the server's reply line. "
                  "Uses a line-based protocol (each exchange is newline "
                  "terminated). Pass --json for machine-readable output "
                  "suitable for agent/MCP integration.",
    .run         = fetch_run,
    .print_usage = fetch_print_usage,
};
