#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "argtable3.h"
#include "cmd_spec.h"

/* --------------------------------------------------------------------------
 * argtable3 builder
 * -------------------------------------------------------------------------- */

static void build_stat_argtable(
    struct arg_lit  **help,
    struct arg_lit  **json,
    struct arg_file **file,
    struct arg_end  **end,
    void            **tbl)         /* caller-allocated array of 5 slots */
{
    *help = arg_lit0("h", "help", "show this help and exit");
    *json = arg_lit0(NULL, "json", "emit machine-readable JSON (for agents/MCP)");
    *file = arg_file1(NULL, NULL, "FILE", "file or directory to stat");
    *end  = arg_end(20);

    tbl[0] = *help;
    tbl[1] = *json;
    tbl[2] = *file;
    tbl[3] = *end;
    tbl[4] = NULL;
}

/* --------------------------------------------------------------------------
 * helpers
 * -------------------------------------------------------------------------- */

/* Build a symbolic permission string like "-rwxr-xr-x" into buf (11 chars + NUL). */
static void mode_str(mode_t m, char buf[12])
{
    buf[0]  = S_ISDIR(m)  ? 'd' : S_ISLNK(m) ? 'l' : S_ISFIFO(m) ? 'p' :
              S_ISSOCK(m) ? 's' : S_ISBLK(m)  ? 'b' : S_ISCHR(m)  ? 'c' : '-';
    buf[1]  = (m & S_IRUSR) ? 'r' : '-';
    buf[2]  = (m & S_IWUSR) ? 'w' : '-';
    buf[3]  = (m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S') : ((m & S_IXUSR) ? 'x' : '-');
    buf[4]  = (m & S_IRGRP) ? 'r' : '-';
    buf[5]  = (m & S_IWGRP) ? 'w' : '-';
    buf[6]  = (m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S') : ((m & S_IXGRP) ? 'x' : '-');
    buf[7]  = (m & S_IROTH) ? 'r' : '-';
    buf[8]  = (m & S_IWOTH) ? 'w' : '-';
    buf[9]  = (m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T') : ((m & S_IXOTH) ? 'x' : '-');
    buf[10] = '\0';
}

/* Format a time_t as "YYYY-MM-DD HH:MM:SS +0000" into buf. */
static void fmt_time(time_t t, char buf[64])
{
    struct tm *tm = localtime(&t);
    strftime(buf, 64, "%Y-%m-%d %H:%M:%S %z", tm);
}

/* Escape a path for JSON (handle backslash and double-quote). */
static void json_escape(const char *in, char *out, size_t outsz)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < outsz; i++) {
        if (in[i] == '"' || in[i] == '\\')
            out[j++] = '\\';
        out[j++] = in[i];
    }
    out[j] = '\0';
}

/* --------------------------------------------------------------------------
 * output functions
 * -------------------------------------------------------------------------- */

static int print_plain(FILE *out, const char *path, const struct stat *st)
{
    char modestr[12];
    char atime[64], mtime[64], ctime_[64];
    mode_str(st->st_mode, modestr);
    fmt_time(st->st_atime, atime);
    fmt_time(st->st_mtime, mtime);
    fmt_time(st->st_ctime, ctime_);

    struct passwd *pw = getpwuid(st->st_uid);
    struct group  *gr = getgrgid(st->st_gid);

    fprintf(out, "  File: %s\n",                path);
    fprintf(out, "  Size: %-15lld Blocks: %-10lld IO Block: %lld\n",
           (long long)st->st_size,
           (long long)st->st_blocks,
           (long long)st->st_blksize);
    fprintf(out, "Device: %-18llu Inode: %-12llu Links: %llu\n",
           (unsigned long long)st->st_dev,
           (unsigned long long)st->st_ino,
           (unsigned long long)st->st_nlink);
    fprintf(out, "Access: (%04o/%s)  Uid: (%5u/%s)  Gid: (%5u/%s)\n",
           st->st_mode & 07777, modestr,
           st->st_uid, pw ? pw->pw_name : "?",
           st->st_gid, gr ? gr->gr_name : "?");
    fprintf(out, "Access: %s\n", atime);
    fprintf(out, "Modify: %s\n", mtime);
    fprintf(out, "Change: %s\n", ctime_);

    return 0;
}

static int print_json(FILE *out, const char *path, const struct stat *st)
{
    char modestr[12];
    char atime[64], mtime[64], ctime_[64];
    mode_str(st->st_mode, modestr);
    fmt_time(st->st_atime, atime);
    fmt_time(st->st_mtime, mtime);
    fmt_time(st->st_ctime, ctime_);

    struct passwd *pw = getpwuid(st->st_uid);
    struct group  *gr = getgrgid(st->st_gid);

    char escaped[4096];
    json_escape(path, escaped, sizeof(escaped));

    fprintf(out, "{\n");
    fprintf(out, "  \"path\":     \"%s\",\n",  escaped);
    fprintf(out, "  \"size\":     %lld,\n",   (long long)st->st_size);
    fprintf(out, "  \"blocks\":   %lld,\n",   (long long)st->st_blocks);
    fprintf(out, "  \"blksize\":  %lld,\n",   (long long)st->st_blksize);
    fprintf(out, "  \"inode\":    %llu,\n",   (unsigned long long)st->st_ino);
    fprintf(out, "  \"device\":   %llu,\n",   (unsigned long long)st->st_dev);
    fprintf(out, "  \"nlink\":    %llu,\n",   (unsigned long long)st->st_nlink);
    fprintf(out, "  \"mode\":     %u,\n",     (unsigned)st->st_mode);
    fprintf(out, "  \"mode_str\": \"%s\",\n", modestr);
    fprintf(out, "  \"uid\":      %u,\n",     st->st_uid);
    fprintf(out, "  \"user\":     \"%s\",\n", pw ? pw->pw_name : "");
    fprintf(out, "  \"gid\":      %u,\n",     st->st_gid);
    fprintf(out, "  \"group\":    \"%s\",\n", gr ? gr->gr_name : "");
    fprintf(out, "  \"atime\":    \"%s\",\n", atime);
    fprintf(out, "  \"mtime\":    \"%s\",\n", mtime);
    fprintf(out, "  \"ctime\":    \"%s\"\n",  ctime_);
    fprintf(out, "}\n");

    return 0;
}

/* --------------------------------------------------------------------------
 * print_usage
 * -------------------------------------------------------------------------- */

void stat_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *json;
    struct arg_file *file;
    struct arg_end  *end;
    void            *tbl[5];

    build_stat_argtable(&help, &json, &file, &end, tbl);

    fprintf(out, "Usage: stat ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nDisplay file status information.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 4);
}

/* --------------------------------------------------------------------------
 * run
 * -------------------------------------------------------------------------- */

int stat_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    (void)in_stream;   /* stat operates on named paths, never reads a stream */

    struct arg_lit  *help;
    struct arg_lit  *json;
    struct arg_file *file;
    struct arg_end  *end;
    void            *tbl[5];

    build_stat_argtable(&help, &json, &file, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        stat_print_usage(out_stream);
        arg_freetable(tbl, 4);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "stat");
        fprintf(stderr, "Try 'stat --help' for more information.\n");
        arg_freetable(tbl, 4);
        return 1;
    }

    const char *path = file->filename[0];
    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, "stat: cannot stat '%s': %s\n", path, strerror(errno));
        arg_freetable(tbl, 4);
        return 1;
    }

    int ret = (json->count > 0) ? print_json(out_stream, path, &st)
                                 : print_plain(out_stream, path, &st);
    arg_freetable(tbl, 4);
    return ret;
}

/* --------------------------------------------------------------------------
 * cmd_spec_t registration
 * -------------------------------------------------------------------------- */

cmd_spec_t cmd_stat_spec = {
    .name       = "stat",
    .summary    = "display file status",
    .long_help  = "Display detailed metadata for a file or directory "
                  "(size, inode, permissions, timestamps, owner). "
                  "Use --json for stable machine-readable output.",
    .run         = stat_run,
    .print_usage = stat_print_usage,
};
