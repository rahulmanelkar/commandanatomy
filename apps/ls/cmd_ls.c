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
    void            **tbl)         /* caller-allocated array of 6 slots */
{
    *help  = arg_lit0("h", "help",  "show this help and exit");
    *all   = arg_lit0("a", "all",   "do not ignore entries starting with '.'");
    *json  = arg_lit0(NULL, "json", "emit machine-readable JSON (for agents/MCP)");
    *paths = arg_filen(NULL, NULL, "[PATH...]", 0, 32, "files or directories to list");
    *end   = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *all;
    tbl[2] = *json;
    tbl[3] = *paths;
    tbl[4] = *end;
    tbl[5] = NULL;
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

/* Escape a string into a JSON string body: double quotes, backslashes, and
 * control characters (incl. newline/CR/tab). Prevents a crafted filename from
 * breaking out of the JSON structure or injecting a payload when a listing is
 * streamed to a remote agent. */
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

/* Thread-safe strerror. With -D_GNU_SOURCE the GNU strerror_r returns a
 * pointer that may or may not be `buf`, so the return value must be used (not
 * `buf`). The caller supplies a stack-local buffer so concurrent clients never
 * race on the static buffer that plain strerror() would share. */
static const char *errstr(int errnum, char *buf, size_t buflen)
{
    return strerror_r(errnum, buf, buflen);
}

/* List one directory in plain text. Returns 0 on success, 1 on error. */
static int list_dir_plain(FILE *out, const char *path, int show_all)
{
    DIR *dp = opendir(path);
    if (!dp) {
        char ebuf[128];
        fprintf(stderr, "ls: cannot open '%s': %s\n",
                path, errstr(errno, ebuf, sizeof ebuf));
        return 1;
    }
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (!show_all && ent->d_name[0] == '.')
            continue;
        fprintf(out, "%s\n", ent->d_name);
    }
    closedir(dp);
    return 0;
}

/* List one directory as JSON. Returns 0 on success, 1 on error. */
static int list_dir_json(FILE *out, const char *path, int show_all)
{
    DIR *dp = opendir(path);
    if (!dp) {
        char ebuf[128];
        fprintf(stderr, "ls: cannot open '%s': %s\n",
                path, errstr(errno, ebuf, sizeof ebuf));
        return 1;
    }

    fprintf(out, "{\n  \"path\": \"");
    json_escape(out, path, strlen(path));
    fprintf(out, "\",\n  \"entries\": [\n");

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

        if (!first) fprintf(out, ",\n");
        fprintf(out, "    {\"name\": \"");
        json_escape(out, ent->d_name, strlen(ent->d_name));
        fprintf(out, "\", \"type\": \"%s\", \"size\": %lld}", type, size);
        first = 0;
    }

    fprintf(out, "\n  ]\n}\n");
    closedir(dp);
    return 0;
}

/* List a single non-directory entry (plain). */
static int list_file_plain(FILE *out, const char *path)
{
    fprintf(out, "%s\n", path);
    return 0;
}

/* List a single non-directory entry (JSON). */
static int list_file_json(FILE *out, const char *path)
{
    struct stat st;
    const char *type = "unknown";
    long long size = -1;
    if (stat(path, &st) == 0) {
        type = entry_type(st.st_mode);
        size = (long long)st.st_size;
    }
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    fprintf(out, "{\n  \"path\": \"");
    json_escape(out, path, strlen(path));
    fprintf(out, "\",\n  \"entries\": [\n");
    fprintf(out, "    {\"name\": \"");
    json_escape(out, name, strlen(name));
    fprintf(out, "\", \"type\": \"%s\", \"size\": %lld}\n", type, size);
    fprintf(out, "  ]\n}\n");
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
    void            *tbl[6];

    build_ls_argtable(&help, &all, &json, &paths, &end, tbl);

    fprintf(out, "Usage: ls ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nList directory contents.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 5);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int ls_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    (void)in_stream;   /* ls never reads input */

    struct arg_lit  *help;
    struct arg_lit  *all;
    struct arg_lit  *json;
    struct arg_file *paths;
    struct arg_end  *end;
    void            *tbl[6];

    build_ls_argtable(&help, &all, &json, &paths, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        ls_print_usage(out_stream);
        arg_freetable(tbl, 5);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "ls");
        fprintf(stderr, "Try 'ls --help' for more information.\n");
        arg_freetable(tbl, 5);
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

    if (use_json && multi) fprintf(out_stream, "[\n");

    int json_first = 1;
    for (int i = 0; i < n; i++) {
        const char *p = targets[i];
        struct stat st;

        if (stat(p, &st) != 0) {
            char ebuf[128];
            fprintf(stderr, "ls: cannot access '%s': %s\n",
                    p, errstr(errno, ebuf, sizeof ebuf));
            ret = 1;
            continue;
        }

        if (use_json && multi) {
            if (!json_first) fprintf(out_stream, ",\n");
            json_first = 0;
        }

        if (S_ISDIR(st.st_mode)) {
            if (multi && !use_json)
                fprintf(out_stream, "%s:\n", p);
            ret |= use_json ? list_dir_json(out_stream, p, show_all)
                            : list_dir_plain(out_stream, p, show_all);
            if (multi && !use_json && i + 1 < n)
                fprintf(out_stream, "\n");
        } else {
            ret |= use_json ? list_file_json(out_stream, p)
                            : list_file_plain(out_stream, p);
        }
    }

    if (use_json && multi) fprintf(out_stream, "]\n");

    arg_freetable(tbl, 5);
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
