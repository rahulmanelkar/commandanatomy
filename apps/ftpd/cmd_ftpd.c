#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>          /* struct timeval for SO_RCVTIMEO */
#include <netinet/in.h>
#include <arpa/inet.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* The daemon reuses two existing command specs by reference, exactly as a
 * shell pipeline would — no fork, no exec. */
extern cmd_spec_t cmd_ls_spec;
extern cmd_spec_t cmd_mkdir_spec;

#define FTPD_DEFAULT_PORT 21021
#define FTPD_DEFAULT_USER "rahulbox"
#define FTPD_DEFAULT_PASS "rahulbox"
#define CTL_LINE_MAX      2048
#define XFER_BUF          65536

/* Per-connection descriptor handed to each worker thread. The expected
 * credentials point at storage that outlives every worker (argtable values or
 * string literals), so they can be shared read-only across threads. */
typedef struct {
    int         ctl_fd;
    int         timeout;      /* idle seconds on the control channel; 0 = none */
    const char *user;         /* username a client must supply */
    const char *pass;         /* password a client must supply */
} conn_ctx_t;

/* Per-session data channel. A transfer is served either actively (connect back
 * to a PORT address) or passively (accept on a listening socket opened by PASV).
 * Lives on the worker's stack, so each client's channel is independent. */
typedef struct {
    int                pasv_fd;     /* passive listening socket, or -1 */
    int                have_port;   /* 1 once a valid PORT address is set */
    struct sockaddr_in port_addr;   /* active-mode target (from PORT) */
    int                timeout;     /* seconds; bounds a passive accept() */
} datachan_t;

/* --------------------------------------------------------------------------
 * argtable3 builder — single source of truth for parsing and help output
 * -------------------------------------------------------------------------- */

static void build_ftpd_argtable(
    struct arg_lit **help,
    struct arg_int **port,
    struct arg_int **timeout,
    struct arg_str **user,
    struct arg_str **pass,
    struct arg_lit **json,
    struct arg_end **end,
    void           **tbl)         /* caller-allocated array of 8 slots */
{
    *help    = arg_lit0("h", "help",    "show this help and exit");
    *port    = arg_int0("p", "port",    "PORT", "listen port (default 21021)");
    *timeout = arg_int0("t", "timeout", "SECS", "idle timeout per connection, seconds (0 = none)");
    *user    = arg_str0("u", "user",    "NAME", "username clients must log in as (default \"rahulbox\")");
    *pass    = arg_str0(NULL, "pass",   "WORD", "password clients must supply (default \"rahulbox\")");
    *json    = arg_lit0(NULL, "json",   "emit a machine-readable JSON status log");
    *end     = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *port;
    tbl[2] = *timeout;
    tbl[3] = *user;
    tbl[4] = *pass;
    tbl[5] = *json;
    tbl[6] = *end;
    tbl[7] = NULL;
}

/* --------------------------------------------------------------------------
 * low-level control/data helpers
 * -------------------------------------------------------------------------- */

/* Write exactly n bytes, retrying short/interrupted writes. Returns 0 / -1.
 * Binary-safe: operates purely on byte counts, so embedded NULs are fine. */
static int write_all(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;          /* EPIPE etc. — SIGPIPE is ignored daemon-wide */
        }
        off += (size_t)w;
    }
    return 0;
}

static void ctl_reply(int fd, const char *line)
{
    write_all(fd, line, strlen(line));
}

/* Read one CRLF-framed control line into buf (NUL-terminated, CRLF stripped).
 * Returns line length (>=0), -1 on EOF/error, -2 on idle timeout. */
static int ctl_readline(int fd, char *buf, size_t cap)
{
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) {                       /* peer closed the connection */
            if (i == 0) return -1;
            break;
        }
        if (r < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -2;  /* timeout */
            return -1;
        }
        if (c == '\n') break;
        buf[i++] = c;
    }
    if (i > 0 && buf[i - 1] == '\r') i--;   /* drop the CR of CRLF */
    buf[i] = '\0';
    return (int)i;
}

/* Decode "a1,a2,a3,a4,p1,p2" into an IPv4 sockaddr. port = p1*256 + p2. */
static int parse_port(const char *arg, struct sockaddr_in *out)
{
    unsigned a1, a2, a3, a4, p1, p2;
    if (sscanf(arg, "%u,%u,%u,%u,%u,%u", &a1, &a2, &a3, &a4, &p1, &p2) != 6)
        return -1;
    if (a1 > 255 || a2 > 255 || a3 > 255 || a4 > 255 || p1 > 255 || p2 > 255)
        return -1;

    memset(out, 0, sizeof *out);
    out->sin_family      = AF_INET;
    out->sin_port        = htons((uint16_t)(p1 * 256 + p2));
    out->sin_addr.s_addr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
    return 0;
}

/* Active-mode data channel: connect back to the client's decoded PORT addr. */
static int data_connect(const struct sockaddr_in *addr)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    if (connect(fd, (const struct sockaddr *)addr, sizeof *addr) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void str_upper(char *s)
{
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* --------------------------------------------------------------------------
 * data channel — active (PORT) or passive (PASV)
 * -------------------------------------------------------------------------- */

/* True if a data channel has been armed by a prior PORT or PASV. */
static int datachan_armed(const datachan_t *dc)
{
    return dc->pasv_fd >= 0 || dc->have_port;
}

/* Close any armed-but-unused data channel (e.g. a stale PASV listener). */
static void datachan_reset(datachan_t *dc)
{
    if (dc->pasv_fd >= 0) { close(dc->pasv_fd); dc->pasv_fd = -1; }
    dc->have_port = 0;
}

/* Establish the actual data connection and consume the channel. In passive mode
 * this accept()s on the PASV listener; in active mode it connect()s to the PORT
 * address. Returns the connected data fd, or -1. */
static int datachan_open(datachan_t *dc)
{
    int fd = -1;
    if (dc->pasv_fd >= 0) {
        fd = accept(dc->pasv_fd, NULL, NULL);
        close(dc->pasv_fd);
        dc->pasv_fd = -1;
    } else if (dc->have_port) {
        fd = data_connect(&dc->port_addr);
    }
    dc->have_port = 0;          /* a PORT/PASV applies to a single transfer */
    return fd;
}

/* PASV: open an ephemeral listening socket on the same local IP the client
 * reached us on, and advertise it as h1,h2,h3,h4,p1,p2 in a 227 reply. */
static void handle_pasv(int ctl, datachan_t *dc)
{
    datachan_reset(dc);         /* drop any previously armed channel */

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { ctl_reply(ctl, "425 Can't open data connection.\r\n"); return; }

    /* Bind to the control connection's local address, so the advertised IP is
     * necessarily one the client can reach; let the kernel pick the port. */
    struct sockaddr_in local;
    socklen_t          llen = sizeof local;
    if (getsockname(ctl, (struct sockaddr *)&local, &llen) < 0) {
        close(lfd); ctl_reply(ctl, "425 Can't open data connection.\r\n"); return;
    }
    local.sin_port = 0;

    if (bind(lfd, (struct sockaddr *)&local, sizeof local) < 0 || listen(lfd, 1) < 0) {
        close(lfd); ctl_reply(ctl, "425 Can't open data connection.\r\n"); return;
    }

    struct sockaddr_in bound;
    socklen_t          blen = sizeof bound;
    if (getsockname(lfd, (struct sockaddr *)&bound, &blen) < 0) {
        close(lfd); ctl_reply(ctl, "425 Can't open data connection.\r\n"); return;
    }

    if (dc->timeout > 0) {       /* bound the eventual accept() so it can't hang */
        struct timeval tv = { .tv_sec = dc->timeout, .tv_usec = 0 };
        setsockopt(lfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }
    dc->pasv_fd = lfd;

    uint32_t ip   = ntohl(local.sin_addr.s_addr);
    uint16_t port = ntohs(bound.sin_port);
    char reply[CTL_LINE_MAX];
    snprintf(reply, sizeof reply,
             "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u).\r\n",
             (ip >> 24) & 0xffu, (ip >> 16) & 0xffu, (ip >> 8) & 0xffu, ip & 0xffu,
             (port >> 8) & 0xffu, port & 0xffu);
    ctl_reply(ctl, reply);
}

/* --------------------------------------------------------------------------
 * command handlers — these reuse the ls/mkdir specs by reference
 * -------------------------------------------------------------------------- */

/* MKD: invoke cmd_mkdir_spec.run with a synthetic argv, capturing its output
 * into a memory stream so the exact diagnostic can drive the FTP reply. */
static void handle_mkd(int ctl, const char *path)
{
    char  *cap    = NULL;
    size_t caplen = 0;
    int    rc     = -1;

    FILE *mem = open_memstream(&cap, &caplen);
    if (mem) {
        char  prog[] = "mkdir";
        char *av[]   = { prog, (char *)path, NULL };
        rc = cmd_mkdir_spec.run(2, av, NULL, mem);   /* out_stream = capture buffer */
        fflush(mem);
        fclose(mem);
    }

    char reply[CTL_LINE_MAX];
    if (rc == 0) {
        snprintf(reply, sizeof reply, "257 \"%s\" created.\r\n", path);
    } else {
        if (cap) {                                   /* trim trailing CR/LF */
            size_t n = strlen(cap);
            while (n && (cap[n - 1] == '\n' || cap[n - 1] == '\r')) cap[--n] = '\0';
        }
        snprintf(reply, sizeof reply, "550 %s\r\n",
                 (cap && *cap) ? cap : "Create directory operation failed.");
    }
    ctl_reply(ctl, reply);
    free(cap);
}

/* LIST: open the data connection, hand the socket to cmd_ls_spec.run as its
 * out_stream, flush+close, then report 226 on the control channel. */
static void handle_list(int ctl, datachan_t *dc)
{
    if (!datachan_armed(dc)) { ctl_reply(ctl, "425 Use PORT or PASV first.\r\n"); return; }

    /* 150 must precede the accept()/connect() so a passive client that waits for
     * 150 before finishing its side of the data connection can't deadlock. */
    ctl_reply(ctl, "150 Here comes the directory listing.\r\n");

    int dfd = datachan_open(dc);
    if (dfd < 0) { ctl_reply(ctl, "426 Can't open data connection.\r\n"); return; }

    FILE *dout = fdopen(dfd, "w");
    if (dout) {
        char  prog[] = "ls";
        char *av[]   = { prog, NULL };               /* lists the daemon's cwd */
        cmd_ls_spec.run(1, av, NULL, dout);          /* out_stream = data socket */
        fflush(dout);
        fclose(dout);                                /* flushes + closes dfd */
    } else {
        close(dfd);
    }
    ctl_reply(ctl, "226 Directory send OK.\r\n");
}

/* RETR: stream a file out to the data connection. Byte-count I/O => NUL-safe. */
static void handle_retr(int ctl, datachan_t *dc, const char *path)
{
    if (!datachan_armed(dc)) { ctl_reply(ctl, "425 Use PORT or PASV first.\r\n"); return; }

    int ffd = open(path, O_RDONLY);
    if (ffd < 0) {
        datachan_reset(dc);                 /* abandon the armed channel */
        ctl_reply(ctl, "550 File not found or not accessible.\r\n");
        return;
    }

    ctl_reply(ctl, "150 Opening BINARY mode data connection.\r\n");

    int dfd = datachan_open(dc);
    if (dfd < 0) { close(ffd); ctl_reply(ctl, "426 Can't open data connection.\r\n"); return; }

    char    buf[XFER_BUF];
    int     ok = 1;
    ssize_t r;
    while ((r = read(ffd, buf, sizeof buf)) > 0) {
        if (write_all(dfd, buf, (size_t)r) < 0) { ok = 0; break; }
    }
    if (r < 0) ok = 0;

    close(ffd);
    close(dfd);
    ctl_reply(ctl, ok ? "226 Transfer complete.\r\n" : "426 Transfer aborted.\r\n");
}

/* STOR: stream from the data connection into a file. NUL-safe by construction. */
static void handle_stor(int ctl, datachan_t *dc, const char *path)
{
    if (!datachan_armed(dc)) { ctl_reply(ctl, "425 Use PORT or PASV first.\r\n"); return; }

    int ffd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (ffd < 0) {
        datachan_reset(dc);                 /* abandon the armed channel */
        ctl_reply(ctl, "550 Cannot create file.\r\n");
        return;
    }

    ctl_reply(ctl, "150 Ok to send data.\r\n");

    int dfd = datachan_open(dc);
    if (dfd < 0) { close(ffd); ctl_reply(ctl, "426 Can't open data connection.\r\n"); return; }

    char    buf[XFER_BUF];
    int     ok = 1;
    ssize_t r;
    while ((r = read(dfd, buf, sizeof buf)) > 0) {
        if (write_all(ffd, buf, (size_t)r) < 0) { ok = 0; break; }
    }
    if (r < 0) ok = 0;

    close(ffd);
    close(dfd);
    ctl_reply(ctl, ok ? "226 Transfer complete.\r\n" : "426 Transfer aborted.\r\n");
}

/* --------------------------------------------------------------------------
 * ConnectionHandler — one worker thread per client; allowlisted state machine
 * -------------------------------------------------------------------------- */

static void *ConnectionHandler(void *arg)
{
    conn_ctx_t *ctx = arg;
    int ctl = ctx->ctl_fd;

    /* Session state — entirely on this thread's stack, isolated per client. */
    int        logged_in  = 0;
    int        user_given = 0;       /* a USER was supplied, awaiting PASS */
    char       acct[128]  = {0};     /* the username from the last USER */
    datachan_t dc;
    memset(&dc, 0, sizeof dc);
    dc.pasv_fd = -1;
    dc.timeout = ctx->timeout;

    if (ctx->timeout > 0) {
        struct timeval tv = { .tv_sec = ctx->timeout, .tv_usec = 0 };
        setsockopt(ctl, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }

    ctl_reply(ctl, "220 rahulbox FTP server ready.\r\n");

    char line[CTL_LINE_MAX];
    for (;;) {
        int len = ctl_readline(ctl, line, sizeof line);
        if (len == -2) { ctl_reply(ctl, "421 Timeout, closing control connection.\r\n"); break; }
        if (len < 0)   break;          /* EOF / error */
        if (len == 0)  continue;       /* empty line */

        /* Split "VERB <arg...>"; only the verb is upper-cased so paths keep case. */
        char *sp  = strchr(line, ' ');
        char *arg = NULL;
        if (sp) { *sp = '\0'; arg = sp + 1; }
        str_upper(line);
        const char *verb = line;

        if (strcmp(verb, "USER") == 0) {
            /* Always ask for a password; never reveal whether the name is valid. */
            snprintf(acct, sizeof acct, "%s", arg ? arg : "");
            user_given = 1;
            logged_in  = 0;            /* a new USER must re-authenticate */
            char reply[CTL_LINE_MAX];
            snprintf(reply, sizeof reply, "331 Password required for %s.\r\n", acct);
            ctl_reply(ctl, reply);
        } else if (strcmp(verb, "PASS") == 0) {
            const char *given = arg ? arg : "";
            if (!user_given) {
                ctl_reply(ctl, "503 Login with USER first.\r\n");
            } else if (strcmp(acct, ctx->user) == 0 && strcmp(given, ctx->pass) == 0) {
                logged_in = 1;
                ctl_reply(ctl, "230 Login successful.\r\n");
            } else {
                logged_in  = 0;
                user_given = 0;        /* force a fresh USER/PASS exchange */
                ctl_reply(ctl, "530 Login incorrect.\r\n");
            }
        } else if (strcmp(verb, "QUIT") == 0) {
            ctl_reply(ctl, "221 Goodbye.\r\n");
            break;                     /* close THIS connection only */
        } else if (strcmp(verb, "NOOP") == 0) {
            ctl_reply(ctl, "200 Command successful.\r\n");
        } else if (strcmp(verb, "TYPE") == 0) {
            ctl_reply(ctl, "200 Type set to binary.\r\n");
        } else if (strcmp(verb, "SYST") == 0) {
            ctl_reply(ctl, "215 UNIX Type: L8\r\n");
        } else if (!logged_in) {
            ctl_reply(ctl, "530 Not logged in.\r\n");
        } else if (strcmp(verb, "PORT") == 0) {
            if (arg && parse_port(arg, &dc.port_addr) == 0) {
                if (dc.pasv_fd >= 0) { close(dc.pasv_fd); dc.pasv_fd = -1; }  /* drop stale PASV */
                dc.have_port = 1;
                ctl_reply(ctl, "200 PORT command successful.\r\n");
            } else {
                ctl_reply(ctl, "501 Syntax error in parameters.\r\n");
            }
        } else if (strcmp(verb, "PASV") == 0) {
            handle_pasv(ctl, &dc);
        } else if (strcmp(verb, "MKD") == 0) {
            if (arg && *arg) handle_mkd(ctl, arg);
            else             ctl_reply(ctl, "501 Syntax error in parameters.\r\n");
        } else if (strcmp(verb, "LIST") == 0 || strcmp(verb, "NLST") == 0) {
            handle_list(ctl, &dc);
        } else if (strcmp(verb, "RETR") == 0) {
            if (arg && *arg) handle_retr(ctl, &dc, arg);
            else             ctl_reply(ctl, "501 Syntax error in parameters.\r\n");
        } else if (strcmp(verb, "STOR") == 0) {
            if (arg && *arg) handle_stor(ctl, &dc, arg);
            else             ctl_reply(ctl, "501 Syntax error in parameters.\r\n");
        } else if (strcmp(verb, "PWD") == 0 || strcmp(verb, "XPWD") == 0) {
            ctl_reply(ctl, "257 \"/\" is the current directory.\r\n");
        } else {
            ctl_reply(ctl, "502 Command not implemented.\r\n");
        }
    }

    datachan_reset(&dc);    /* close any passive listener still open */
    close(ctl);
    free(ctx);
    return NULL;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void ftpd_print_usage(FILE *out)
{
    struct arg_lit *help, *json;
    struct arg_int *port, *timeout;
    struct arg_str *user, *pass;
    struct arg_end *end;
    void           *tbl[8];

    build_ftpd_argtable(&help, &port, &timeout, &user, &pass, &json, &end, tbl);

    fprintf(out, "Usage: ftpd ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nServe the current directory over a minimal multi-threaded FTP daemon.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 7);
}

/* --------------------------------------------------------------------------
 * run — passive listening socket + accept loop, one thread per connection
 * -------------------------------------------------------------------------- */

int ftpd_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    (void)in_stream;   /* the daemon reads from sockets, not the shell stream */

    struct arg_lit *help, *json;
    struct arg_int *port, *timeout;
    struct arg_str *user, *pass;
    struct arg_end *end;
    void           *tbl[8];

    build_ftpd_argtable(&help, &port, &timeout, &user, &pass, &json, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        ftpd_print_usage(out_stream);
        arg_freetable(tbl, 7);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "ftpd");
        fprintf(stderr, "Try 'ftpd --help' for more information.\n");
        arg_freetable(tbl, 7);
        return 1;
    }

    int listen_port = port->count    > 0 ? port->ival[0]    : FTPD_DEFAULT_PORT;
    int conn_to     = timeout->count > 0 ? timeout->ival[0] : 0;
    int use_json    = json->count    > 0;

    /* Expected credentials. These point into argtable/literal storage that lives
     * for the whole daemon, so worker threads can share them read-only. */
    const char *expect_user = user->count > 0 ? user->sval[0] : FTPD_DEFAULT_USER;
    const char *expect_pass = pass->count > 0 ? pass->sval[0] : FTPD_DEFAULT_PASS;

    if (listen_port < 1 || listen_port > 65535) {
        fprintf(stderr, "ftpd: invalid port %d\n", listen_port);
        arg_freetable(tbl, 7);
        return 1;
    }

    /* A write to a client that vanished must not take down the whole daemon. */
    signal(SIGPIPE, SIG_IGN);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        fprintf(stderr, "ftpd: socket: %s\n", strerror(errno));
        arg_freetable(tbl, 7);
        return 1;
    }

    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port        = htons((uint16_t)listen_port);

    if (bind(srv, (struct sockaddr *)&sa, sizeof sa) < 0) {
        fprintf(stderr, "ftpd: bind port %d: %s\n", listen_port, strerror(errno));
        close(srv);
        arg_freetable(tbl, 7);
        return 1;
    }
    if (listen(srv, 16) < 0) {
        fprintf(stderr, "ftpd: listen: %s\n", strerror(errno));
        close(srv);
        arg_freetable(tbl, 7);
        return 1;
    }

    if (use_json)
        fprintf(out_stream, "{\"event\":\"listening\",\"port\":%d,\"timeout\":%d,\"user\":\"%s\"}\n",
                listen_port, conn_to, expect_user);
    else
        fprintf(out_stream, "ftpd: listening on port %d (user '%s', timeout %ds; Ctrl-C to stop)\n",
                listen_port, expect_user, conn_to);
    fflush(out_stream);

    /* Accept loop: spawn a detached worker per client, then loop straight back
     * to accept() so the daemon stays responsive to new connections. */
    for (;;) {
        struct sockaddr_in peer;
        socklen_t          plen = sizeof peer;
        int cfd = accept(srv, (struct sockaddr *)&peer, &plen);
        if (cfd < 0) {
            if (errno == EINTR || errno == ECONNABORTED) continue;
            fprintf(stderr, "ftpd: accept: %s\n", strerror(errno));
            break;
        }

        conn_ctx_t *ctx = malloc(sizeof *ctx);
        if (!ctx) { close(cfd); continue; }
        ctx->ctl_fd  = cfd;
        ctx->timeout = conn_to;
        ctx->user    = expect_user;
        ctx->pass    = expect_pass;

        if (use_json)
            fprintf(out_stream, "{\"event\":\"accept\",\"peer\":\"%s:%d\"}\n",
                    inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
        else
            fprintf(out_stream, "ftpd: connection from %s:%d\n",
                    inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
        fflush(out_stream);

        pthread_t tid;
        if (pthread_create(&tid, NULL, ConnectionHandler, ctx) != 0) {
            fprintf(stderr, "ftpd: pthread_create: %s\n", strerror(errno));
            close(cfd);
            free(ctx);
            continue;
        }
        pthread_detach(tid);    /* fire-and-forget: resources reclaimed on exit */
    }

    close(srv);
    arg_freetable(tbl, 7);
    return 1;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_ftpd_spec = {
    .name       = "ftpd",
    .summary    = "multi-threaded active-mode FTP server",
    .long_help  = "Serve the current working directory over a minimal, multi-threaded "
                  "FTP daemon. Each client is handled on its own detached pthread with "
                  "stack-isolated session state. Supports USER, QUIT, PORT, PASV, LIST, "
                  "NLST, MKD, RETR and STOR; LIST and MKD reuse the in-process ls and "
                  "mkdir command specs. Both active (PORT) and passive (PASV) data "
                  "connections work, so standard FTP clients (ftp, curl) connect "
                  "out-of-the-box. Clients authenticate with USER/PASS against the "
                  "credentials set by --user/--pass (default rahulbox/rahulbox). "
                  "Transfers are binary-safe.",
    .run         = ftpd_run,
    .print_usage = ftpd_print_usage,
};
