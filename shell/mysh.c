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
#include "tok.h"

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
