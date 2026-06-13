/*
 * mysh — a small Unix shell
 *
 * Features:
 *   - cmd_spec_t registry: hello, ls, stat, wc, cat run in-process
 *   - Built-ins: cd, exit/quit, help
 *   - External commands via PATH (fork + execvp)
 *   - I/O redirection: <  >  >>
 *   - Pipelines: |
 *   - Single and double quoting
 *   - Comment lines starting with #
 *   - Script mode: mysh script.sh  (or pipe/redirect stdin)
 *   - ~/.mysh/bin automatically prepended to PATH on startup
 *   - Interactive line editing: Left/Right arrows, Home/End, Delete,
 *     kill shortcuts (linedit.c); scripts and pipes still use fgets
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>

#include "../include/cmd_spec.h"
#include "argtable3.h"      /* arg_hdr introspection for MCP inputSchema */
#define JSMN_HEADER         /* declarations only; implementation links from jsmn.o */
#include "jsmn.h"
#include "tok.h"
#include "linedit.h"

/* ── external cmd_spec_t exports from app modules ───────────────────────── */

extern cmd_spec_t cmd_hello_spec;
extern cmd_spec_t cmd_ls_spec;
extern cmd_spec_t cmd_stat_spec;
extern cmd_spec_t cmd_wc_spec;
extern cmd_spec_t cmd_cat_spec;
extern cmd_spec_t cmd_echo_spec;
extern cmd_spec_t cmd_head_spec;
extern cmd_spec_t cmd_tail_spec;
extern cmd_spec_t cmd_grep_spec;
extern cmd_spec_t cmd_sort_spec;
extern cmd_spec_t cmd_uniq_spec;
extern cmd_spec_t cmd_cut_spec;
extern cmd_spec_t cmd_tee_spec;
extern cmd_spec_t cmd_pkg_spec;
extern cmd_spec_t cmd_fetch_spec;
extern cmd_spec_t cmd_mkdir_spec;
extern cmd_spec_t cmd_ftpd_spec;

/* ── natural-language '@' mode ───────────────────────────────────────────────
 * An interactive line beginning with '@' bypasses pipeline parsing: the rest
 * of the line is forwarded verbatim, as the fetch MESSAGE, to a local AI mock
 * server. */
#define AI_HOST "localhost"
#define AI_PORT "5001"
/* When set and non-empty, its value is passed to fetch as `-t SECS` so the AI
 * round-trip can wait longer than fetch's 5s default without affecting plain
 * `fetch` calls. fetch validates the value (integer 0-3600; 0 = no limit). */
#define AI_TIMEOUT_ENV "RAHULBOX_AI_TIMEOUT"

/*
 * When the shell reads commands from a piped/redirected stdin (not a tty and
 * not a named script file), we dup stdin to a fresh fd and read from that.
 * This leaves fd 0 clean for forked children so they inherit an unmolested
 * stdin rather than a FILE* buffer full of buffered command text.
 */
static int g_cmd_fd = -1;   /* set in main(), closed in every fork child */

/* ── in-shell command registry ──────────────────────────────────────────── */

#define REGISTRY_MAX 64
static cmd_spec_t *registry[REGISTRY_MAX];
static int         registry_n = 0;

static void reg_register(cmd_spec_t *s)
{
    if (registry_n < REGISTRY_MAX) registry[registry_n++] = s;
}

static cmd_spec_t *reg_find(const char *name)
{
    for (int i = 0; i < registry_n; i++)
        if (strcmp(registry[i]->name, name) == 0) return registry[i];
    return NULL;
}

static void reg_print_all(void)
{
    for (int i = 0; i < registry_n; i++)
        printf("  %-16s %s\n", registry[i]->name, registry[i]->summary);
}

/* ── --commands-json: export the registry as an MCP-style tool catalog ───────
 *
 * Emit a single top-level JSON array describing every registered command so an
 * AI agent can discover the shell's capabilities without running anything. Each
 * element is { "name", "summary", "usage": [ ...lines... ] }, where the usage
 * lines are captured from the command's own print_usage() — the same argtable3
 * help text a human sees — so the catalog and --help can never drift apart.
 */

/* Write s[0..len) to `out` as a JSON string literal (quotes included), escaping
 * the characters JSON requires plus every C0 control char as \u00XX. Raw UTF-8
 * bytes (>= 0x80) pass through verbatim, which is valid JSON. */
static void json_write_string(FILE *out, const char *s, size_t len)
{
    fputc('"', out);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\b': fputs("\\b",  out); break;
            case '\f': fputs("\\f",  out); break;
            case '\n': fputs("\\n",  out); break;
            case '\r': fputs("\\r",  out); break;
            case '\t': fputs("\\t",  out); break;
            default:
                if (c < 0x20) fprintf(out, "\\u%04x", c);
                else          fputc((int)c, out);
        }
    }
    fputc('"', out);
}

/* Run spec->print_usage() into an in-memory buffer and return it (caller frees);
 * *out_len receives the byte count. Returns NULL on allocation failure or if the
 * command has no print_usage. */
static char *capture_usage(const cmd_spec_t *spec, size_t *out_len)
{
    *out_len = 0;
    if (!spec->print_usage) return NULL;

    char  *buf  = NULL;
    size_t size = 0;
    FILE  *mem  = open_memstream(&buf, &size);   /* glibc, needs _GNU_SOURCE */
    if (!mem) return NULL;

    spec->print_usage(mem);
    fclose(mem);                                  /* flushes; buf/size now set */
    *out_len = size;
    return buf;
}

/* Emit the captured usage text as a JSON array of trimmed, non-blank lines onto
 * `out`. Leading indentation is stripped and whitespace-only lines dropped so
 * the array is a clean, flat list of usage/option lines. */
static void json_write_usage_array(FILE *out, const char *text, size_t len)
{
    fputs("[", out);
    int first = 1;
    size_t start = 0;
    for (size_t j = 0; j <= len; j++) {
        if (j != len && text[j] != '\n') continue;

        /* line is text[start..j); drop a trailing CR if present */
        size_t s = start, e = j;
        if (e > s && text[e - 1] == '\r') e--;
        /* trim leading whitespace */
        while (s < e && (text[s] == ' ' || text[s] == '\t')) s++;

        /* skip blank lines */
        int blank = 1;
        for (size_t k = s; k < e; k++)
            if (!isspace((unsigned char)text[k])) { blank = 0; break; }

        if (!blank) {
            fputs(first ? "\n        " : ",\n        ", out);
            json_write_string(out, text + s, e - s);
            first = 0;
        }
        start = j + 1;
    }
    fputs(first ? "]" : "\n      ]", out);   /* "[]" when no usage lines */
}

/* Print the whole registry as one valid JSON array on stdout. */
static void dump_commands_json(void)
{
    printf("[");
    for (int i = 0; i < registry_n; i++) {
        const cmd_spec_t *spec = registry[i];
        const char *name    = spec->name    ? spec->name    : "";
        const char *summary = spec->summary ? spec->summary : "";

        printf("%s\n  {\n", i ? "," : "");

        printf("    \"name\": ");
        json_write_string(stdout, name, strlen(name));
        printf(",\n    \"summary\": ");
        json_write_string(stdout, summary, strlen(summary));

        size_t ulen = 0;
        char  *usage = capture_usage(spec, &ulen);
        printf(",\n    \"usage\": ");
        json_write_usage_array(stdout, usage ? usage : "", ulen);
        free(usage);

        printf("\n  }");
    }
    printf("%s]\n", registry_n ? "\n" : "");
}

/* ── --mcp-tools: emit a spec-compliant MCP tools/list result ────────────────
 *
 * Where --commands-json emits human help text, this derives a typed JSON-Schema
 * `inputSchema` per command by introspecting its argtable3 objects via the
 * cmd_spec_t.build_argtable hook. Type mapping:
 *     arg_lit (no value)         -> "boolean"
 *     arg_int                    -> "integer"
 *     arg_dbl                    -> "number"
 *     arg_str / arg_file / other -> "string"
 *     any arg with mincount > 0  -> added to "required"
 * Every allocation is stack-local and freed before return, so this is safe to
 * run on a worker thread (no static/global state). */

#define MCP_ARGTABLE_MAX 32

/* JSON property name for one arg: first long option, else the short-option
 * char, else (a positional) a lowercased alphanumeric squeeze of its datatype,
 * e.g. "[FILE...]" -> "file", "PATTERN" -> "pattern". */
static void mcp_prop_name(const struct arg_hdr *h, char *buf, size_t cap)
{
    size_t i = 0;
    if (h->longopts && *h->longopts) {
        for (const char *p = h->longopts; *p && *p != ',' && i + 1 < cap; p++)
            buf[i++] = *p;
    } else if (h->shortopts && *h->shortopts) {
        buf[i++] = h->shortopts[0];
    } else {
        const char *dt = h->datatype ? h->datatype : "";
        for (const char *p = dt; *p && i + 1 < cap; p++)
            if (isalnum((unsigned char)*p))
                buf[i++] = (char)tolower((unsigned char)*p);
    }
    if (i == 0 && cap > 3) { buf[i++] = 'a'; buf[i++] = 'r'; buf[i++] = 'g'; }
    buf[i] = '\0';
}

/* Skip the arg_end terminator and the universal --help flag (not a tool arg). */
static int mcp_is_property(const struct arg_hdr *h)
{
    if (!h || (h->flag & ARG_TERMINATOR)) return 0;
    if (h->longopts && strcmp(h->longopts, "help") == 0) return 0;
    return 1;
}

static void write_mcp_input_schema(FILE *out, const cmd_spec_t *spec)
{
    /* No introspection hook: still emit a valid (open) object schema. */
    if (!spec->build_argtable) {
        fputs("{ \"type\": \"object\", \"properties\": {} }", out);
        return;
    }

    void *tbl[MCP_ARGTABLE_MAX];
    int n = spec->build_argtable(tbl, MCP_ARGTABLE_MAX);
    if (n < 0) {
        fputs("{ \"type\": \"object\", \"properties\": {} }", out);
        return;
    }

    /* argtable3 assigns one fixed scanfn per value type, so pointer-equality
     * classifies an arg even when the builder overrode its datatype string. */
    struct arg_int *ref_int = arg_int0(NULL, "i", NULL, NULL);
    struct arg_dbl *ref_dbl = arg_dbl0(NULL, "d", NULL, NULL);
    arg_scanfn *int_scan = ref_int ? ref_int->hdr.scanfn : NULL;
    arg_scanfn *dbl_scan = ref_dbl ? ref_dbl->hdr.scanfn : NULL;

    fputs("{\n        \"type\": \"object\",\n        \"properties\": {", out);

    int props = 0;
    for (int i = 0; i < n; i++) {
        struct arg_hdr *h = (struct arg_hdr *)tbl[i];   /* hdr is the first member */
        if (!mcp_is_property(h)) continue;

        const char *type;
        if (!(h->flag & ARG_HASVALUE))               type = "boolean"; /* arg_lit */
        else if (int_scan && h->scanfn == int_scan)  type = "integer";
        else if (dbl_scan && h->scanfn == dbl_scan)  type = "number";
        else                                         type = "string";  /* str/file */

        char name[64];
        mcp_prop_name(h, name, sizeof name);

        fputs(props++ ? ",\n          " : "\n          ", out);
        json_write_string(out, name, strlen(name));
        fputs(": { \"type\": ", out);
        json_write_string(out, type, strlen(type));
        if (h->glossary && *h->glossary) {
            fputs(", \"description\": ", out);
            json_write_string(out, h->glossary, strlen(h->glossary));
        }
        fputs(" }", out);
    }
    fputs(props ? "\n        }" : "}", out);

    /* required[]: any arg with mincount > 0 (covers required positionals). */
    int reqs = 0;
    for (int i = 0; i < n; i++) {
        struct arg_hdr *h = (struct arg_hdr *)tbl[i];
        if (!mcp_is_property(h) || h->mincount <= 0) continue;
        char name[64];
        mcp_prop_name(h, name, sizeof name);
        fputs(reqs++ ? ", " : ",\n        \"required\": [", out);
        json_write_string(out, name, strlen(name));
    }
    if (reqs) fputc(']', out);

    fputs("\n      }", out);

    arg_freetable(tbl, (size_t)n);             /* free the command's table */
    void *reftab[2]; int rn = 0;               /* and the reference objects */
    if (ref_int) reftab[rn++] = ref_int;
    if (ref_dbl) reftab[rn++] = ref_dbl;
    arg_freetable(reftab, (size_t)rn);
}

/* Emit the MCP ListToolsResult: { "tools": [ {name, description, inputSchema} ] }.
 * The JSON-RPC 2.0 envelope (jsonrpc/id/result) is added by the Phase 2 server
 * dispatch; this is the `result` payload a `tools/list` call returns. */
static void dump_mcp_tools(void)
{
    printf("{\n  \"tools\": [");
    for (int i = 0; i < registry_n; i++) {
        const cmd_spec_t *spec = registry[i];
        const char *name = spec->name ? spec->name : "";
        const char *desc = spec->summary ? spec->summary : "";

        printf("%s\n    {\n      \"name\": ", i ? "," : "");
        json_write_string(stdout, name, strlen(name));
        printf(",\n      \"description\": ");
        json_write_string(stdout, desc, strlen(desc));
        printf(",\n      \"inputSchema\": ");
        write_mcp_input_schema(stdout, spec);
        printf("\n    }");
    }
    printf("%s]\n}\n", registry_n ? "\n  " : "");
}

/* --jsmn-selftest: prove the vendored parser links and runs (Phase 2 will use
 * it to parse inbound JSON-RPC). Zero-allocation: tokens live on the stack. */
static int mcp_jsmn_selftest(void)
{
    static const char *sample =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{}}";
    jsmn_parser p;
    jsmntok_t   tok[32];
    jsmn_init(&p);
    int n = jsmn_parse(&p, sample, strlen(sample), tok, 32);
    if (n < 0) { fprintf(stderr, "jsmn: parse error %d\n", n); return 1; }
    printf("jsmn: parsed %d tokens from a sample JSON-RPC request\n", n);
    for (int i = 0; i + 1 < n; i++) {
        int len = tok[i].end - tok[i].start;
        if (tok[i].type == JSMN_STRING && len == 6 &&
            strncmp(sample + tok[i].start, "method", 6) == 0) {
            printf("jsmn: method = %.*s\n",
                   tok[i + 1].end - tok[i + 1].start, sample + tok[i + 1].start);
            break;
        }
    }
    return 0;
}

/* ── pipeline structures ────────────────────────────────────────────────── */

#define PIPELINE_MAX 16

typedef struct {
    char *argv[TOK_MAX];
    int   argc;
    char *redir_in;    /* filename for <,  or NULL */
    char *redir_out;   /* filename for > or >>,  or NULL */
    int   append;      /* 1 if >> */
} stage_t;

/* ── parse tokens into pipeline stages ──────────────────────────────────── */

static int parse_pipeline(tok_t *tok, stage_t *stages, int *nstages)
{
    *nstages = 0;
    if (tok->n == 0) return 0;

    stage_t *cur = &stages[0];
    memset(cur, 0, sizeof *cur);
    *nstages = 1;

    for (int i = 0; i < tok->n; i++) {
        char *w = tok->w[i];

        if (strcmp(w, "|") == 0) {
            if (cur->argc == 0) {
                fprintf(stderr, "mysh: syntax error near '|'\n");
                return -1;
            }
            cur->argv[cur->argc] = NULL;
            if (*nstages >= PIPELINE_MAX) {
                fprintf(stderr, "mysh: too many pipe stages\n");
                return -1;
            }
            cur = &stages[(*nstages)++];
            memset(cur, 0, sizeof *cur);

        } else if (strcmp(w, "<") == 0) {
            if (++i >= tok->n) { fprintf(stderr, "mysh: expected filename after '<'\n"); return -1; }
            cur->redir_in = tok->w[i];

        } else if (strcmp(w, ">") == 0) {
            if (++i >= tok->n) { fprintf(stderr, "mysh: expected filename after '>'\n"); return -1; }
            cur->redir_out = tok->w[i];
            cur->append    = 0;

        } else if (strcmp(w, ">>") == 0) {
            if (++i >= tok->n) { fprintf(stderr, "mysh: expected filename after '>>'\n"); return -1; }
            cur->redir_out = tok->w[i];
            cur->append    = 1;

        } else {
            if (cur->argc >= TOK_MAX - 1) {
                fprintf(stderr, "mysh: too many arguments\n");
                return -1;
            }
            cur->argv[cur->argc++] = w;
        }
    }
    cur->argv[cur->argc] = NULL;
    if (*nstages > 1 && cur->argc == 0) {
        fprintf(stderr, "mysh: syntax error near '|'\n");
        return -1;
    }

    if (stages[0].argc == 0) { *nstages = 0; return 0; }
    return 0;
}

/* ── I/O redirection helpers ─────────────────────────────────────────────── */

static int apply_redirs(const stage_t *s)
{
    if (s->redir_in) {
        int fd = open(s->redir_in, O_RDONLY);
        if (fd < 0) { fprintf(stderr, "mysh: %s: %s\n", s->redir_in, strerror(errno)); return -1; }
        dup2(fd, STDIN_FILENO); close(fd);
    }
    if (s->redir_out) {
        int flags = O_WRONLY | O_CREAT | (s->append ? O_APPEND : O_TRUNC);
        int fd = open(s->redir_out, flags, 0644);
        if (fd < 0) { fprintf(stderr, "mysh: %s: %s\n", s->redir_out, strerror(errno)); return -1; }
        dup2(fd, STDOUT_FILENO); close(fd);
    }
    return 0;
}

/* ── per-thread argument packet ──────────────────────────────────────────── */

typedef struct {
    cmd_spec_t *spec;
    int         argc;
    char      **argv;        /* points into tok_t; valid while pipeline runs */
    FILE       *in;
    FILE       *out;
    int         exit_code;
} stage_arg_t;

static void *stage_thread_fn(void *vp)
{
    stage_arg_t *a = vp;
    a->exit_code = a->spec->run(a->argc, a->argv, a->in, a->out);
    fclose(a->out);   /* flush and signal EOF to the downstream stage */
    fclose(a->in);
    return a;
}

/* ── run a single stage in-process (no pipe, no concurrency) ─────────────── */

static int run_inproc(stage_t *s)
{
    cmd_spec_t *spec = reg_find(s->argv[0]);

    if (spec) {
        /* Internal command: open any redirected files and pass explicit streams. */
        FILE *in  = stdin;
        FILE *out = stdout;
        FILE *owned_in  = NULL;
        FILE *owned_out = NULL;

        if (s->redir_in) {
            in = owned_in = fopen(s->redir_in, "r");
            if (!in) {
                fprintf(stderr, "mysh: %s: %s\n", s->redir_in, strerror(errno));
                return 1;
            }
        }
        if (s->redir_out) {
            const char *mode = s->append ? "a" : "w";
            out = owned_out = fopen(s->redir_out, mode);
            if (!out) {
                fprintf(stderr, "mysh: %s: %s\n", s->redir_out, strerror(errno));
                if (owned_in) fclose(owned_in);
                return 1;
            }
        }

        int rc = spec->run(s->argc, s->argv, in, out);
        fflush(out);
        if (owned_out) fclose(owned_out);
        if (owned_in)  fclose(owned_in);
        return rc;

    } else {
        /* External command: fork so the child can exec without disturbing us. */
        fflush(stdout);
        fflush(stderr);

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGPIPE, SIG_DFL);
            if (g_cmd_fd >= 0) close(g_cmd_fd);
            if (apply_redirs(s) < 0) _exit(1);
            execvp(s->argv[0], s->argv);
            fprintf(stderr, "mysh: %s: %s\n", s->argv[0], strerror(errno));
            _exit(127);
        }

        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

/* ── run a full pipeline ─────────────────────────────────────────────────── */

/*
 * Per-stage tracking entry.  Exactly one of (is_thread, pid) is live after
 * the stage has been successfully spawned.
 */
typedef struct {
    int          is_thread;
    pthread_t    tid;
    stage_arg_t *arg;        /* heap-allocated; freed after pthread_join */
    pid_t        pid;
} spawn_t;

static int run_pipeline(stage_t *stages, int nstages)
{
    if (nstages == 1) return run_inproc(&stages[0]);

    spawn_t spawned[PIPELINE_MAX];
    memset(spawned, 0, sizeof spawned);
    int n_spawned = 0;

    /*
     * next_in_fd — the read end of the pipe that connects the stage we just
     * set up to the one we are about to set up.  -1 means "no pending fd"
     * (either we haven't created a pipe yet, or we broke out of the loop).
     */
    int next_in_fd = -1;

    for (int i = 0; i < nstages; i++) {
        stage_t    *s    = &stages[i];
        cmd_spec_t *spec = reg_find(s->argv[0]);
        int         last = (i == nstages - 1);

        /* ── this stage's input fd ── */
        int cur_in_fd;
        if (i == 0) {
            if (s->redir_in) {
                cur_in_fd = open(s->redir_in, O_RDONLY);
                if (cur_in_fd < 0) {
                    fprintf(stderr, "mysh: %s: %s\n", s->redir_in, strerror(errno));
                    break;
                }
                fcntl(cur_in_fd, F_SETFD, FD_CLOEXEC);
            } else if (spec) {
                /* Internal thread: dup stdin so fclose in the thread doesn't
                 * close the shell's real fd 0. */
                cur_in_fd = dup(STDIN_FILENO);
                if (cur_in_fd < 0) { perror("dup"); break; }
                fcntl(cur_in_fd, F_SETFD, FD_CLOEXEC);
            } else {
                cur_in_fd = STDIN_FILENO;  /* external: child gets fd 0 directly */
            }
        } else {
            cur_in_fd  = next_in_fd;   /* read end left over from previous pipe */
            next_in_fd = -1;
        }

        /* ── this stage's output fd; also creates next stage's input fd ── */
        int cur_out_fd;

        if (!last) {
            /*
             * Create the inter-stage pipe.  O_CLOEXEC ensures that any
             * fork'd external-command child exec's with these fds closed,
             * preventing it from holding a write end open and blocking EOF.
             */
            int pfd[2];
            if (pipe(pfd) < 0) {
                perror("pipe");
                if (cur_in_fd != STDIN_FILENO) close(cur_in_fd);
                break;
            }
            fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
            fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
            cur_out_fd = pfd[1];
            next_in_fd = pfd[0];
        } else {
            /* Last stage: honour output redirection or dup stdout. */
            if (s->redir_out) {
                int flags = O_WRONLY | O_CREAT | (s->append ? O_APPEND : O_TRUNC);
                cur_out_fd = open(s->redir_out, flags, 0644);
                if (cur_out_fd < 0) {
                    fprintf(stderr, "mysh: %s: %s\n", s->redir_out, strerror(errno));
                    if (cur_in_fd != STDIN_FILENO) close(cur_in_fd);
                    break;
                }
                fcntl(cur_out_fd, F_SETFD, FD_CLOEXEC);
            } else if (spec) {
                /* Internal thread: dup stdout so fclose doesn't close fd 1. */
                cur_out_fd = dup(STDOUT_FILENO);
                if (cur_out_fd < 0) {
                    perror("dup");
                    if (cur_in_fd != STDIN_FILENO) close(cur_in_fd);
                    break;
                }
                fcntl(cur_out_fd, F_SETFD, FD_CLOEXEC);
            } else {
                cur_out_fd = STDOUT_FILENO;
            }
        }

        /* ── spawn the stage ── */
        if (spec) {
            /*
             * Internal command — run inside a worker thread.
             * fdopen() takes ownership of the fds: the thread closes them via
             * fclose() when it finishes, which signals EOF to the next stage.
             */
            stage_arg_t *a = malloc(sizeof *a);
            if (!a) {
                perror("malloc");
                if (cur_in_fd  != STDIN_FILENO)  close(cur_in_fd);
                if (cur_out_fd != STDOUT_FILENO) close(cur_out_fd);
                break;
            }
            a->spec      = spec;
            a->argc      = s->argc;
            a->argv      = s->argv;
            a->exit_code = 0;

            a->in = fdopen(cur_in_fd, "r");
            if (!a->in) {
                perror("fdopen");
                close(cur_in_fd);
                if (cur_out_fd != STDOUT_FILENO) close(cur_out_fd);
                free(a);
                break;
            }
            a->out = fdopen(cur_out_fd, "w");
            if (!a->out) {
                perror("fdopen");
                fclose(a->in);          /* also closes cur_in_fd */
                close(cur_out_fd);
                free(a);
                break;
            }

            spawned[i].is_thread = 1;
            spawned[i].arg       = a;
            if (pthread_create(&spawned[i].tid, NULL, stage_thread_fn, a) != 0) {
                perror("pthread_create");
                fclose(a->in);
                fclose(a->out);
                free(a);
                spawned[i].is_thread = 0;
                break;
            }
            n_spawned++;

        } else {
            /*
             * External command — fork and exec.
             * All pipe fds carry FD_CLOEXEC so exec() closes them in the
             * child automatically; we only dup2 the ones this stage owns.
             */
            fflush(stdout);
            fflush(stderr);

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                if (cur_in_fd  != STDIN_FILENO)  close(cur_in_fd);
                if (cur_out_fd != STDOUT_FILENO) close(cur_out_fd);
                break;
            }

            if (pid == 0) {
                signal(SIGINT,  SIG_DFL);
                signal(SIGPIPE, SIG_DFL);
                if (g_cmd_fd >= 0) close(g_cmd_fd);

                /* dup2 lands the fds on 0/1; the originals close with the
                 * FD_CLOEXEC copies on exec, leaving a clean fd table. */
                if (cur_in_fd  != STDIN_FILENO)
                    { dup2(cur_in_fd,  STDIN_FILENO);  close(cur_in_fd); }
                if (cur_out_fd != STDOUT_FILENO)
                    { dup2(cur_out_fd, STDOUT_FILENO); close(cur_out_fd); }

                execvp(s->argv[0], s->argv);
                fprintf(stderr, "mysh: %s: %s\n", s->argv[0], strerror(errno));
                _exit(127);
            }

            /* Parent: release fds now that the child has them. */
            if (cur_in_fd  != STDIN_FILENO)  close(cur_in_fd);
            if (cur_out_fd != STDOUT_FILENO) close(cur_out_fd);

            spawned[i].pid = pid;
            n_spawned++;
        }
    }

    /* If we broke out of the loop early, discard the dangling read-end fd. */
    if (next_in_fd >= 0) close(next_in_fd);

    /*
     * Collect results.  Join all threads (including intermediate ones) before
     * reaping processes, so that thread-generated writes reach downstream
     * readers before we declare the pipeline done.
     *
     * pthread_join on the final thread gives us the terminal exit status.
     */
    int last_rc = 1;
    for (int i = 0; i < n_spawned; i++) {
        if (spawned[i].is_thread) {
            pthread_join(spawned[i].tid, NULL);
            int rc = spawned[i].arg->exit_code;
            free(spawned[i].arg);
            if (i == n_spawned - 1) last_rc = rc;
        } else {
            int status;
            waitpid(spawned[i].pid, &status, 0);
            int rc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            if (i == n_spawned - 1) last_rc = rc;
        }
    }
    return last_rc;
}

/* ── built-in: help ──────────────────────────────────────────────────────── */

static void print_help(void)
{
    printf("Built-in commands:\n");
    printf("  %-16s %s\n", "cd [DIR]",   "change working directory (default: $HOME)");
    printf("  %-16s %s\n", "exit [N]",   "exit the shell with optional status N");
    printf("  %-16s %s\n", "help",        "show this help");
    printf("\nRegistered commands:\n");
    reg_print_all();
    printf("\nAll other commands are looked up in PATH.\n");
}

/* ── startup: prepend ~/.mysh/bin to PATH if it exists ──────────────────── */

static void setup_path(void)
{
    const char *home = getenv("HOME");
    if (!home) return;

    char mysh_bin[4096];
    snprintf(mysh_bin, sizeof mysh_bin, "%s/.mysh/bin", home);

    struct stat st;
    if (stat(mysh_bin, &st) != 0 || !S_ISDIR(st.st_mode)) return;

    const char *old = getenv("PATH");
    if (!old) old = "/usr/local/bin:/usr/bin:/bin";

    /* Avoid double-prepending on re-exec or nested shells. */
    if (strstr(old, mysh_bin)) return;

    char new_path[8192];
    snprintf(new_path, sizeof new_path, "%s:%s", mysh_bin, old);
    setenv("PATH", new_path, 1);
}

/* ── '@' natural-language mode: Suggest → Confirm → Execute ──────────────────
 *
 * contains_ci: case-insensitive substring test used only by the offline
 * heuristic below. */
static int contains_ci(const char *hay, const char *needle)
{
    size_t nl = strlen(needle);
    if (nl == 0) return 1;
    for (const char *h = hay; *h; h++) {
        size_t i = 0;
        while (i < nl && tolower((unsigned char)h[i]) ==
                         tolower((unsigned char)needle[i]))
            i++;
        if (i == nl) return 1;
    }
    return 0;
}

/*
 * Deterministic, offline fallback for when the AI gateway can't be reached.
 * Maps a few keywords in the prompt to a suggested rahulbox pipeline so the
 * user still gets a useful pointer with no network round-trip. Never executes
 * anything — without the gateway there is nothing trustworthy to confirm, so we
 * only print a hint.
 */
static void ai_fallback_hint(const char *prompt, FILE *out)
{
    const char *hint;
    if      (contains_ci(prompt, "count") || contains_ci(prompt, "how many"))
        hint = "wc FILE";
    else if (contains_ci(prompt, "search") || contains_ci(prompt, "find") ||
             contains_ci(prompt, "contain") || contains_ci(prompt, "match") ||
             contains_ci(prompt, "grep"))
        hint = "grep PATTERN FILE   (or: cat FILE | grep PATTERN)";
    else if (contains_ci(prompt, "sort"))
        hint = "sort FILE   (add | uniq to drop duplicates)";
    else if (contains_ci(prompt, "first") || contains_ci(prompt, "head") ||
             contains_ci(prompt, "top"))
        hint = "head FILE";
    else if (contains_ci(prompt, "last") || contains_ci(prompt, "tail") ||
             contains_ci(prompt, "end"))
        hint = "tail FILE";
    else if (contains_ci(prompt, "show") || contains_ci(prompt, "read") ||
             contains_ci(prompt, "print") || contains_ci(prompt, "content") ||
             contains_ci(prompt, "cat"))
        hint = "cat FILE";
    else if (contains_ci(prompt, "list") || contains_ci(prompt, "file") ||
             contains_ci(prompt, "dir"))
        hint = "ls   (add -a to include hidden entries)";
    else
        hint = "help   (lists every command rahulbox can run)";

    fprintf(out, "mysh (offline hint)> %s\n", hint);
    fflush(out);
}

/*
 * Ask the gateway: run fetch in-process with its reply captured into a heap
 * buffer (open_memstream) instead of printed. fetch reports transport failures
 * on stderr and returns non-zero, so the return code distinguishes a real reply
 * from a dead connection. *out_reply receives a trimmed, NUL-terminated copy
 * (caller frees; may be empty/NULL). The "--" guards prompts starting with '-';
 * $RAHULBOX_AI_TIMEOUT, if set, becomes fetch's -t SECS. Returns fetch's code,
 * or -1 if the capture buffer could not be created.
 */
static int ai_query(const char *prompt, FILE *in, char **out_reply)
{
    *out_reply = NULL;

    char  *buf  = NULL;
    size_t size = 0;
    FILE  *cap  = open_memstream(&buf, &size);
    if (!cap) return -1;

    const char *timeout = getenv(AI_TIMEOUT_ENV);
    char *fargv[10];
    int   fargc = 0;
    fargv[fargc++] = "fetch";
    fargv[fargc++] = "-H";
    fargv[fargc++] = AI_HOST;
    fargv[fargc++] = "-p";
    fargv[fargc++] = AI_PORT;
    if (timeout && *timeout) {
        fargv[fargc++] = "-t";
        fargv[fargc++] = (char *)timeout;
    }
    fargv[fargc++] = "--";
    fargv[fargc++] = (char *)prompt;
    fargv[fargc]   = NULL;

    int rc = cmd_fetch_spec.run(fargc, fargv, in, cap);
    fclose(cap);                 /* flush; buf/size now valid */

    /* Strip the trailing CR/LF/space the line protocol leaves on the reply. */
    while (size > 0 && (buf[size - 1] == '\n' || buf[size - 1] == '\r' ||
                        buf[size - 1] == ' '  || buf[size - 1] == '\t'))
        buf[--size] = '\0';

    *out_reply = buf;
    return rc;
}

/*
 * The '@' interactive hook: a Suggest → Confirm → Execute state machine.
 *
 *   1. Suggest  ask the gateway (fetch, in-process) and capture its one line of
 *               advice rather than printing it.
 *   2. Confirm  echo it under a "mysh (AI Suggestion)>" banner and require an
 *               explicit y/Y at a [y/N] gate before anything runs.
 *   3. Execute  on y, feed the EXACT suggested line through the same engine a
 *               typed command uses: tok_split() → parse_pipeline() →
 *               run_pipeline(). Any other key — including a bare Enter — prints
 *               "Aborted." and returns 1 without running a thing.
 *
 * If fetch can't reach the gateway (non-zero exit) or the gateway returns no
 * usable command, the gate is bypassed and a deterministic offline heuristic
 * hint is printed instead. Returns the executed pipeline's status, or 1 on
 * abort / gateway failure.
 */
static int run_at_prompt(const char *prompt, FILE *in, FILE *out)
{
    /* ── 1. Suggestion phase ─────────────────────────────────────────────── */
    char *reply = NULL;
    int   frc   = ai_query(prompt, in, &reply);

    const char *sug = reply ? reply : "";
    while (*sug == ' ' || *sug == '\t') sug++;   /* skip leading whitespace */

    /* ── 4. Deterministic fallback: unreachable gateway / no command ─────── */
    int unreachable = (frc != 0);
    int no_command  = (!unreachable &&
                       (*sug == '\0' || strncmp(sug, "rahulbox AI", 11) == 0));
    if (unreachable || no_command) {
        if (unreachable)
            fprintf(stderr,
                    "mysh: @: AI gateway unreachable (fetch exited %d).\n", frc);
        else
            fprintf(stderr, "mysh: @: AI gateway returned no command%s%s\n",
                    (*sug ? ": " : "."), sug);
        ai_fallback_hint(prompt, out);
        free(reply);
        return 1;
    }

    /* ── 2. Confirmation gate ────────────────────────────────────────────── */
    fprintf(out, "mysh (AI Suggestion)> %s\n", sug);
    fflush(out);

    char answer[64];
    if (linedit_read(in, "Execute this command? [y/N]: ",
                     answer, sizeof answer) < 0) {
        fprintf(out, "\nAborted.\n");            /* EOF / Ctrl-D at the gate */
        free(reply);
        return 1;
    }

    const char *a = answer;
    while (*a == ' ' || *a == '\t') a++;
    if (*a != 'y' && *a != 'Y') {
        fprintf(out, "Aborted.\n");
        free(reply);
        return 1;
    }

    /* ── 3. Execution phase: route the exact line through the real engine ── */
    fprintf(out, "Executing: %s\n", sug);
    fflush(out);

    tok_t tok;
    if (tok_split(sug, &tok, 0) < 0) {
        fprintf(stderr, "mysh: @: could not tokenize suggestion "
                        "(unmatched quote?)\n");
        free(reply);
        return 1;
    }
    if (tok.n == 0) {
        tok_free(&tok);
        free(reply);
        return 1;
    }

    stage_t stages[PIPELINE_MAX];
    int nstages = 0;
    if (parse_pipeline(&tok, stages, &nstages) < 0 || nstages == 0) {
        fprintf(stderr, "mysh: @: malformed pipeline in suggestion\n");
        tok_free(&tok);
        free(reply);
        return 1;
    }

    int status = run_pipeline(stages, nstages);
    tok_free(&tok);
    free(reply);
    return status;
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    /* Register all built-in command modules. */
    reg_register(&cmd_hello_spec);
    reg_register(&cmd_ls_spec);
    reg_register(&cmd_stat_spec);
    reg_register(&cmd_wc_spec);
    reg_register(&cmd_cat_spec);
    reg_register(&cmd_echo_spec);
    reg_register(&cmd_head_spec);
    reg_register(&cmd_tail_spec);
    reg_register(&cmd_grep_spec);
    reg_register(&cmd_sort_spec);
    reg_register(&cmd_uniq_spec);
    reg_register(&cmd_cut_spec);
    reg_register(&cmd_tee_spec);
    reg_register(&cmd_pkg_spec);
    reg_register(&cmd_fetch_spec);
    reg_register(&cmd_mkdir_spec);
    reg_register(&cmd_ftpd_spec);

    /* ── --commands-json: boot-time tool-catalog export ─────────────────────
     * Must run after the registry is fully populated but before any
     * interactive/script setup. Dump the registry as JSON and exit 0 so an AI
     * agent can ingest the catalog with a plain `mysh --commands-json`. */
    if (argc >= 2 && strcmp(argv[1], "--commands-json") == 0) {
        dump_commands_json();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--mcp-tools") == 0) {
        dump_mcp_tools();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--jsmn-selftest") == 0) {
        return mcp_jsmn_selftest();
    }

    setup_path();

    /* SIGINT: shell ignores it; each child restores SIG_DFL before exec. */
    signal(SIGINT,  SIG_IGN);
    /* SIGPIPE: ignore broken pipes in the shell process itself. */
    signal(SIGPIPE, SIG_IGN);

    /* Script mode: mysh script.sh */
    FILE *input = stdin;
    if (argc >= 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            fprintf(stderr, "mysh: %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        /*
         * stdin is a pipe or redirect.  Dup it to a fresh fd and read
         * commands from there, leaving fd 0 untouched.  Forked pipeline
         * children then inherit a clean fd 0 with no buffered command text.
         */
        g_cmd_fd = dup(STDIN_FILENO);
        if (g_cmd_fd >= 0) {
            input = fdopen(g_cmd_fd, "r");
            if (!input) { close(g_cmd_fd); g_cmd_fd = -1; /* fall back */ }
        }
    }

    int interactive = isatty(fileno(input));
    int last_status = 0;

    /* Seed OLDPWD so 'cd -' works from the very first cd. */
    {
        char cwd[4096];
        if (getcwd(cwd, sizeof cwd)) setenv("OLDPWD", cwd, 0);
    }

    if (interactive) {
        printf("mysh — type 'help' for available commands, 'exit' to quit\n");
    }

    char line[4096];

    while (1) {
        /* Reap any finished background jobs to avoid zombies. */
        while (waitpid(-1, NULL, WNOHANG) > 0);

        /* Read the next line: raw-mode line editor at an interactive
         * prompt (arrow keys, Home/End, ...), plain fgets for scripts
         * and piped input. */
        if (interactive) {
            char prompt[32];
            if (last_status != 0)
                snprintf(prompt, sizeof prompt, "mysh [%d]> ", last_status);
            else
                snprintf(prompt, sizeof prompt, "mysh> ");

            if (linedit_read(input, prompt, line, sizeof line) < 0) {
                printf("\n");
                break;   /* EOF: Ctrl-D at an empty prompt */
            }
        } else if (!fgets(line, sizeof line, input)) {
            break;       /* end of script file / piped input */
        }

        /* Strip trailing newline */
        line[strcspn(line, "\n")] = '\0';

        /* ── natural-language '@' prefix ────────────────────────────────
         * If the first non-whitespace character is '@', bypass tokenization
         * and pipeline parsing entirely.  The remainder of the line is a raw
         * prompt forwarded to the AI mock server via fetch. */
        {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;   /* first non-whitespace */
            if (*p == '@') {
                p++;                                   /* skip '@' */
                while (*p == ' ' || *p == '\t') p++;   /* skip leading spaces */

                if (!interactive) {
                    /* '@' is an interactive convenience; ignore it in scripts
                     * and piped input so non-interactive runs stay offline
                     * and deterministic. */
                    fprintf(stderr, "mysh: @: natural-language mode is "
                                    "interactive only; ignoring\n");
                    last_status = 0;
                    continue;
                }

                /* Strip one optional pair of surrounding quotes. */
                size_t plen = strlen(p);
                if (plen >= 2 &&
                    ((p[0] == '"'  && p[plen - 1] == '"') ||
                     (p[0] == '\'' && p[plen - 1] == '\''))) {
                    p[plen - 1] = '\0';
                    p++;
                }

                if (*p == '\0') {
                    fprintf(stderr, "mysh: @: empty prompt\n");
                    last_status = 1;
                    continue;
                }

                last_status = run_at_prompt(p, input, stdout);
                fflush(stdout);
                continue;
            }
        }

        /* Tokenize (with variable expansion) */
        tok_t tok;
        if (tok_split(line, &tok, last_status) < 0) continue;
        if (tok.n == 0) { tok_free(&tok); continue; }

        /* Check for trailing '&' (background execution). */
        int bg = 0;
        if (tok.n > 0 && strcmp(tok.w[tok.n - 1], "&") == 0) {
            bg = 1;
            free(tok.w[tok.n - 1]);
            tok.w[--tok.n] = NULL;
            if (tok.n == 0) { tok_free(&tok); continue; }
        }

        /* Parse pipeline */
        stage_t stages[PIPELINE_MAX];
        int nstages = 0;
        if (parse_pipeline(&tok, stages, &nstages) < 0 || nstages == 0) {
            tok_free(&tok);
            continue;
        }

        /* ── single-stage built-ins (must run in-process) ──────────────── */
        if (nstages == 1) {
            const char *cmd = stages[0].argv[0];

            /* VAR=value: environment variable assignment */
            {
                char *eq = strchr(stages[0].argv[0], '=');
                if (eq && eq > stages[0].argv[0] && stages[0].argc == 1) {
                    *eq = '\0';
                    const char *vname = stages[0].argv[0];
                    int valid = (isalpha((unsigned char)*vname) || *vname == '_');
                    for (const char *vp = vname + 1; valid && *vp; vp++)
                        if (!isalnum((unsigned char)*vp) && *vp != '_') valid = 0;
                    if (valid) {
                        setenv(vname, eq + 1, 1);
                        last_status = 0;
                        tok_free(&tok);
                        continue;
                    }
                    *eq = '=';  /* restore if not a valid assignment */
                }
            }

            if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
                int code = (stages[0].argc >= 2) ? atoi(stages[0].argv[1]) : last_status;
                tok_free(&tok);
                if (input != stdin) fclose(input);
                return code;
            }

            if (strcmp(cmd, "cd") == 0) {
                const char *arg = (stages[0].argc >= 2) ? stages[0].argv[1] : NULL;
                const char *dir;

                if (!arg) {
                    dir = getenv("HOME");
                    if (!dir) { fprintf(stderr, "mysh: cd: HOME not set\n"); last_status = 1; tok_free(&tok); continue; }
                } else if (strcmp(arg, "-") == 0) {
                    dir = getenv("OLDPWD");
                    if (!dir) { fprintf(stderr, "mysh: cd: OLDPWD not set\n"); last_status = 1; tok_free(&tok); continue; }
                    printf("%s\n", dir);  /* bash prints the new directory for cd - */
                } else {
                    dir = arg;
                }

                char prev[4096];
                if (!getcwd(prev, sizeof prev)) prev[0] = '\0';

                if (chdir(dir) < 0) {
                    fprintf(stderr, "mysh: cd: %s: %s\n", dir, strerror(errno));
                    last_status = 1;
                } else {
                    if (prev[0]) setenv("OLDPWD", prev, 1);
                    last_status = 0;
                }
                tok_free(&tok);
                continue;
            }

            if (strcmp(cmd, "help") == 0) {
                print_help();
                last_status = 0;
                tok_free(&tok);
                continue;
            }
        }

        /* ── execute ──────────────────────────────────────────────────── */
        if (bg) {
            fflush(stdout);
            fflush(stderr);
            pid_t bg_pid = fork();
            if (bg_pid < 0) {
                perror("fork");
                last_status = 1;
            } else if (bg_pid == 0) {
                signal(SIGINT, SIG_DFL);
                if (g_cmd_fd >= 0) close(g_cmd_fd);
                int rc = run_pipeline(stages, nstages);
                exit(rc);
            } else {
                printf("[bg] %d\n", bg_pid);
                last_status = 0;
            }
        } else {
            last_status = run_pipeline(stages, nstages);
        }
        tok_free(&tok);
    }

    if (input != stdin) fclose(input);
    return last_status;
}
