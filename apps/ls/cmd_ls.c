#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder — single source of truth for parsing and help output
 * -------------------------------------------------------------------------- */

static void build_ls_argtable(
    struct arg_lit  **help,
    struct arg_lit  **all,
    struct arg_lit  **json,
    struct arg_file **paths,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help  = arg_lit0("h", "help",  "show this help and exit");
    *all   = arg_lit0("a", "all",   "do not ignore entries starting with '.'");
    *json  = arg_lit0(NULL, "json", "emit machine-readable JSON (for agents/MCP)");
    *paths = arg_filen(NULL, NULL, "[PATH...]", 0, 32, "files or directories to list");
    *end   = arg_end(20);

    static void *tbl[6];
    tbl[0] = *help;
    tbl[1] = *all;
    tbl[2] = *json;
    tbl[3] = *paths;
    tbl[4] = *end;
    tbl[5] = NULL;

    *argtable_out = tbl;
}

/* --------------------------------------------------------------------------
 * helpers
 * -------------------------------------------------------------------------- */

static const char *entry_type(mode_t m)
{
    if (S_ISDIR(m))  return "directory";
    if (S_ISLNK(m))  return "symlink";
    if (S_ISFIFO(m)) return "fifo";
    if (S_ISSOCK(m)) return "socket";
    if (S_ISBLK(m))  return "block";
    if (S_ISCHR(m))  return "char";
    return "file";
}

/* List one directory in plain text. Returns 0 on success, 1 on error. */
static int list_dir_plain(const char *path, int show_all)
{
    DIR *dp = opendir(path);
    if (!dp) {
        fprintf(stderr, "ls: cannot open '%s': %s\n", path, strerror(errno));
        return 1;
    }
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (!show_all && ent->d_name[0] == '.')
            continue;
        printf("%s\n", ent->d_name);
    }
    closedir(dp);
    return 0;
}

/* List one directory as JSON. Returns 0 on success, 1 on error. */
static int list_dir_json(const char *path, int show_all)
{
    DIR *dp = opendir(path);
    if (!dp) {
        fprintf(stderr, "ls: cannot open '%s': %s\n", path, strerror(errno));
        return 1;
    }

    printf("{\n  \"path\": \"%s\",\n  \"entries\": [\n", path);

    struct dirent *ent;
    int first = 1;
    while ((ent = readdir(dp)) != NULL) {
        if (!show_all && ent->d_name[0] == '.')
            continue;

        /* stat for type and size */
        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
        struct stat st;
        const char *type = "unknown";
        long long size = -1;
        if (stat(fullpath, &st) == 0) {
            type = entry_type(st.st_mode);
            size = (long long)st.st_size;
        }

        if (!first) printf(",\n");
        printf("    {\"name\": \"%s\", \"type\": \"%s\", \"size\": %lld}",
               ent->d_name, type, size);
        first = 0;
    }

    printf("\n  ]\n}\n");
    closedir(dp);
    return 0;
}

/* List a single non-directory entry (plain). */
static int list_file_plain(const char *path)
{
    printf("%s\n", path);
    return 0;
}

/* List a single non-directory entry (JSON). */
static int list_file_json(const char *path)
{
    struct stat st;
    const char *type = "unknown";
    long long size = -1;
    if (stat(path, &st) == 0) {
        type = entry_type(st.st_mode);
        size = (long long)st.st_size;
    }
    printf("{\n  \"path\": \"%s\",\n  \"entries\": [\n", path);
    printf("    {\"name\": \"%s\", \"type\": \"%s\", \"size\": %lld}\n",
           path, type, size);
    printf("  ]\n}\n");
    return 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void ls_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *all;
    struct arg_lit  *json;
    struct arg_file *paths;
    struct arg_end  *end;
    void           **argtable;

    build_ls_argtable(&help, &all, &json, &paths, &end, &argtable);

    fprintf(out, "Usage: ls ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nList directory contents.\n\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");

    arg_freetable(argtable, 5);
}

/* --------------------------------------------------------------------------
 * run  (skeleton — filesystem logic to be added in next step)
 * -------------------------------------------------------------------------- */

int ls_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *all;
    struct arg_lit  *json;
    struct arg_file *paths;
    struct arg_end  *end;
    void           **argtable;

    build_ls_argtable(&help, &all, &json, &paths, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        ls_print_usage(stdout);
        arg_freetable(argtable, 5);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "ls");
        fprintf(stderr, "Try 'ls --help' for more information.\n");
        arg_freetable(argtable, 5);
        return 1;
    }

    int show_all  = all->count  > 0;
    int use_json  = json->count > 0;
    int ret       = 0;

    /* Default to current directory when no paths given. */
    int n = paths->count;
    const char *default_path[] = { "." };
    const char **targets = (n > 0) ? paths->filename : default_path;
    if (n == 0) n = 1;

    int multi = (n > 1);   /* print header per directory when listing several */

    for (int i = 0; i < n; i++) {
        const char *p = targets[i];
        struct stat st;

        if (stat(p, &st) != 0) {
            fprintf(stderr, "ls: cannot access '%s': %s\n", p, strerror(errno));
            ret = 1;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (multi && !use_json)
                printf("%s:\n", p);
            ret |= use_json ? list_dir_json(p, show_all)
                            : list_dir_plain(p, show_all);
            if (multi && !use_json && i + 1 < n)
                printf("\n");
        } else {
            ret |= use_json ? list_file_json(p)
                            : list_file_plain(p);
        }
    }

    arg_freetable(argtable, 5);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_ls_spec = {
    .name       = "ls",
    .summary    = "list directory contents",
    .long_help  = "List information about files and directories. "
                  "Entries starting with '.' are hidden unless -a is given. "
                  "Use --json for stable machine-readable output.",
    .run         = ls_run,
    .print_usage = ls_print_usage,
};
