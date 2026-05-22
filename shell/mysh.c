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
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "../include/cmd_spec.h"
#include "tok.h"

/* ── external cmd_spec_t exports from app modules ───────────────────────── */

extern cmd_spec_t cmd_hello_spec;
extern cmd_spec_t cmd_ls_spec;
extern cmd_spec_t cmd_stat_spec;
extern cmd_spec_t cmd_wc_spec;
extern cmd_spec_t cmd_cat_spec;

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

/* ── run a single stage in-process (no pipe, registry command) ───────────── */

static int run_inproc(stage_t *s)
{
    /* Flush any buffered output from previous in-process commands before
     * potentially redirecting stdout, so prior output lands in the right place. */
    fflush(stdout);
    fflush(stderr);

    /* Save and redirect fds so the registered command sees the right streams. */
    int saved_in  = -1, saved_out = -1;

    if (s->redir_in) {
        saved_in = dup(STDIN_FILENO);
        int fd = open(s->redir_in, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "mysh: %s: %s\n", s->redir_in, strerror(errno));
            return 1;
        }
        dup2(fd, STDIN_FILENO); close(fd);
        clearerr(stdin);   /* clear EOF flag left by any prior in-process stdin read */
    }
    if (s->redir_out) {
        saved_out = dup(STDOUT_FILENO);
        int flags = O_WRONLY | O_CREAT | (s->append ? O_APPEND : O_TRUNC);
        int fd = open(s->redir_out, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "mysh: %s: %s\n", s->redir_out, strerror(errno));
            if (saved_in >= 0) { dup2(saved_in, STDIN_FILENO); close(saved_in); }
            return 1;
        }
        dup2(fd, STDOUT_FILENO); close(fd);
    }

    cmd_spec_t *spec = reg_find(s->argv[0]);
    int rc;
    if (spec) {
        rc = spec->run(s->argc, s->argv);
        fflush(stdout);
        fflush(stderr);
    } else {
        /* External command: fork so the child can exec without disturbing us. */
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGPIPE, SIG_DFL);
            execvp(s->argv[0], s->argv);
            fprintf(stderr, "mysh: %s: %s\n", s->argv[0], strerror(errno));
            _exit(127);
        }
        int status;
        waitpid(pid, &status, 0);
        rc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }

    if (saved_in  >= 0) { dup2(saved_in,  STDIN_FILENO);  close(saved_in);  }
    if (saved_out >= 0) { dup2(saved_out, STDOUT_FILENO); close(saved_out); }
    return rc;
}

/* ── run a full pipeline ─────────────────────────────────────────────────── */

static int run_pipeline(stage_t *stages, int nstages)
{
    if (nstages == 1) return run_inproc(&stages[0]);

    int     prev_rd = STDIN_FILENO;
    pid_t   pids[PIPELINE_MAX];

    for (int i = 0; i < nstages; i++) {
        int wr_fd   = STDOUT_FILENO;
        int next_rd = -1;

        if (i < nstages - 1) {
            int pfd[2];
            if (pipe(pfd) < 0) { perror("pipe"); return 1; }
            wr_fd   = pfd[1];
            next_rd = pfd[0];
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }

        if (pid == 0) {
            signal(SIGINT,  SIG_DFL);
            signal(SIGPIPE, SIG_DFL);

            if (prev_rd != STDIN_FILENO)  { dup2(prev_rd, STDIN_FILENO);  close(prev_rd);  }
            if (wr_fd   != STDOUT_FILENO) { dup2(wr_fd,   STDOUT_FILENO); close(wr_fd);    }
            if (next_rd >= 0) close(next_rd);

            if (apply_redirs(&stages[i]) < 0) _exit(1);

            cmd_spec_t *spec = reg_find(stages[i].argv[0]);
            if (spec) {
                int r = spec->run(stages[i].argc, stages[i].argv);
                fflush(stdout);
                fflush(stderr);
                _exit(r);
            }

            execvp(stages[i].argv[0], stages[i].argv);
            fprintf(stderr, "mysh: %s: %s\n", stages[i].argv[0], strerror(errno));
            _exit(127);
        }

        pids[i] = pid;
        if (prev_rd != STDIN_FILENO) close(prev_rd);
        if (wr_fd   != STDOUT_FILENO) close(wr_fd);
        prev_rd = next_rd;
    }

    /* Wait for all stages; return exit status of the last one. */
    int last = 0;
    for (int i = 0; i < nstages; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == nstages - 1)
            last = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    return last;
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

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    /* Register all built-in command modules. */
    reg_register(&cmd_hello_spec);
    reg_register(&cmd_ls_spec);
    reg_register(&cmd_stat_spec);
    reg_register(&cmd_wc_spec);
    reg_register(&cmd_cat_spec);

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
    }

    int interactive = isatty(fileno(input));
    int last_status = 0;

    if (interactive) {
        printf("mysh — type 'help' for available commands, 'exit' to quit\n");
    }

    char line[4096];

    while (1) {
        /* Prompt */
        if (interactive) {
            if (last_status != 0)
                printf("mysh [%d]> ", last_status);
            else
                printf("mysh> ");
            fflush(stdout);
        }

        if (!fgets(line, sizeof line, input)) {
            if (interactive) printf("\n");
            break;   /* EOF (Ctrl-D in interactive, end of script file) */
        }

        /* Strip trailing newline */
        line[strcspn(line, "\n")] = '\0';

        /* Tokenize */
        tok_t tok;
        if (tok_split(line, &tok) < 0) continue;
        if (tok.n == 0) { tok_free(&tok); continue; }

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

            if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
                int code = (stages[0].argc >= 2) ? atoi(stages[0].argv[1]) : last_status;
                tok_free(&tok);
                if (input != stdin) fclose(input);
                return code;
            }

            if (strcmp(cmd, "cd") == 0) {
                const char *dir = (stages[0].argc >= 2)
                                  ? stages[0].argv[1]
                                  : getenv("HOME");
                if (!dir) {
                    fprintf(stderr, "mysh: cd: HOME not set\n");
                    last_status = 1;
                } else if (chdir(dir) < 0) {
                    fprintf(stderr, "mysh: cd: %s: %s\n", dir, strerror(errno));
                    last_status = 1;
                } else {
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
        last_status = run_pipeline(stages, nstages);
        tok_free(&tok);
    }

    if (input != stdin) fclose(input);
    return last_status;
}
