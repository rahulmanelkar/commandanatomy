#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* Directories are created with this mode; the process umask is applied by the
 * kernel, so the effective mode is DIR_MODE & ~umask (standard mkdir(1)). */
#define DIR_MODE 0777

/* --------------------------------------------------------------------------
 * argtable3 builder — single source of truth for parsing and help output.
 * Every arg_* object is heap-allocated here and tracked through the caller's
 * stack-local `tbl`, so each invocation owns a private table: two threads
 * running mkdir_run() concurrently (e.g. two FTP clients issuing MKD) never
 * share parser state.
 * -------------------------------------------------------------------------- */

static void build_mkdir_argtable(
    struct arg_lit  **help,
    struct arg_lit  **parents,
    struct arg_file **dirs,
    struct arg_end  **end,
    void            **tbl)          /* caller-allocated array of 5 slots */
{
    *help    = arg_lit0("h", "help",    "show this help and exit");
    *parents = arg_lit0("p", "parents", "make parent directories as needed; "
                                        "no error if the target already exists");
    *dirs    = arg_filen(NULL, NULL, "<directory>", 1, 32, "directory(ies) to create");
    *end     = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *parents;
    tbl[2] = *dirs;
    tbl[3] = *end;
    tbl[4] = NULL;
}

/* --------------------------------------------------------------------------
 * helpers
 * -------------------------------------------------------------------------- */

/* Thread-safe strerror. With -D_GNU_SOURCE the GNU strerror_r returns a
 * pointer that may or may not be `buf`, so the return value must be used (not
 * `buf`). Using the reentrant form keeps concurrent MKD/LIST requests from
 * racing on the static buffer that plain strerror() would share. */
static const char *errstr(int errnum, char *buf, size_t buflen)
{
    return strerror_r(errnum, buf, buflen);
}

/* Create a single directory. Diagnostics go to `out`. Returns 0 / -1. */
static int make_one(const char *dir, FILE *out)
{
    char ebuf[128];
    if (mkdir(dir, DIR_MODE) != 0) {
        fprintf(out, "mkdir: cannot create directory '%s': %s\n",
                dir, errstr(errno, ebuf, sizeof ebuf));
        return -1;
    }
    return 0;
}

/* Create `dir` and any missing parents (mkdir -p). An already-existing
 * component is not an error. Diagnostics go to `out`. Returns 0 / -1. */
static int make_parents(const char *dir, FILE *out)
{
    char tmp[4096];
    char ebuf[128];

    if ((size_t)snprintf(tmp, sizeof tmp, "%s", dir) >= sizeof tmp) {
        fprintf(out, "mkdir: cannot create directory '%s': path too long\n", dir);
        return -1;
    }

    /* Create each intermediate prefix, then the full path. Start at index 1 so
     * a leading '/' (absolute path root) is never passed to mkdir(). */
    for (size_t i = 1; tmp[i]; i++) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (mkdir(tmp, DIR_MODE) != 0 && errno != EEXIST) {
            fprintf(out, "mkdir: cannot create directory '%s': %s\n",
                    tmp, errstr(errno, ebuf, sizeof ebuf));
            return -1;
        }
        tmp[i] = '/';
    }
    if (mkdir(tmp, DIR_MODE) != 0 && errno != EEXIST) {
        fprintf(out, "mkdir: cannot create directory '%s': %s\n",
                tmp, errstr(errno, ebuf, sizeof ebuf));
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void mkdir_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *parents;
    struct arg_file *dirs;
    struct arg_end  *end;
    void            *tbl[5];

    build_mkdir_argtable(&help, &parents, &dirs, &end, tbl);

    fprintf(out, "Usage: mkdir ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nCreate the DIRECTORY(ies), if they do not already exist.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 4);
}

/* --------------------------------------------------------------------------
 * run
 *
 * All output — normal diagnostics AND parse errors — is routed through the
 * passed-in `out_stream`, never the process-global stdout/stderr. This lets
 * the FTP daemon map mkdir onto a network connection: it can capture the exact
 * failure text for the MKD reply, and concurrent clients can't interleave on a
 * shared stderr.
 * -------------------------------------------------------------------------- */

int mkdir_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    (void)in_stream;   /* mkdir never reads input */

    struct arg_lit  *help;
    struct arg_lit  *parents;
    struct arg_file *dirs;
    struct arg_end  *end;
    void            *tbl[5];

    build_mkdir_argtable(&help, &parents, &dirs, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        mkdir_print_usage(out_stream);
        arg_freetable(tbl, 4);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(out_stream, end, "mkdir");
        fprintf(out_stream, "Try 'mkdir --help' for more information.\n");
        arg_freetable(tbl, 4);
        return 1;
    }

    int use_parents = parents->count > 0;
    int ret         = 0;

    /* Process every operand; report a non-zero status if any one fails. */
    for (int i = 0; i < dirs->count; i++) {
        const char *dir = dirs->filename[i];
        int rc = use_parents ? make_parents(dir, out_stream)
                             : make_one(dir, out_stream);
        if (rc != 0) ret = 1;
    }

    arg_freetable(tbl, 4);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_mkdir_spec = {
    .name       = "mkdir",
    .summary    = "create directories",
    .long_help  = "Create each named DIRECTORY if it does not already exist. "
                  "With -p, also create any missing parent directories and treat "
                  "an existing target as success. All diagnostics are written to "
                  "the command's output stream rather than stderr, so the command "
                  "can be mapped onto a network connection (e.g. the FTP MKD verb).",
    .run         = mkdir_run,
    .print_usage = mkdir_print_usage,
};
