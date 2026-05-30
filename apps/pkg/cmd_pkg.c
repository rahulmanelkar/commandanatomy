#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "argtable3.h"
#include "cmd_spec.h"

/* ── constants ───────────────────────────────────────────────────────────── */

#define MAX_BINS      32
#define MAX_BIN_LEN  256
#define MAX_FIELD    128
#define PATHBUF     8192

/* ── types ───────────────────────────────────────────────────────────────── */

typedef struct {
    char name[MAX_FIELD];
    char version[MAX_FIELD];
    char bins[MAX_BINS][MAX_BIN_LEN];
    int  nbin;
} pkg_meta_t;

/* ── argtable3 builders ──────────────────────────────────────────────────── */

static void build_build_argtable(
    struct arg_lit **help,
    struct arg_str **src,
    struct arg_str **output,
    struct arg_end **end,
    void **tbl)          /* caller allocates: void *tbl[5] */
{
    *help   = arg_lit0("h", "help", "show this help and exit");
    *src    = arg_str1(NULL, NULL, "<src-dir>",       "directory to pack");
    *output = arg_str1(NULL, NULL, "<output.tar.gz>", "output archive path");
    *end    = arg_end(20);
    tbl[0] = *help; tbl[1] = *src; tbl[2] = *output; tbl[3] = *end; tbl[4] = NULL;
}

static void build_install_argtable(
    struct arg_lit **help,
    struct arg_str **archive,
    struct arg_end **end,
    void **tbl)          /* caller allocates: void *tbl[4] */
{
    *help    = arg_lit0("h", "help", "show this help and exit");
    *archive = arg_str1(NULL, NULL, "<pkg.tar.gz>", "package archive to install");
    *end     = arg_end(20);
    tbl[0] = *help; tbl[1] = *archive; tbl[2] = *end; tbl[3] = NULL;
}

static void build_list_argtable(
    struct arg_lit **help,
    struct arg_end **end,
    void **tbl)          /* caller allocates: void *tbl[3] */
{
    *help = arg_lit0("h", "help", "show this help and exit");
    *end  = arg_end(20);
    tbl[0] = *help; tbl[1] = *end; tbl[2] = NULL;
}

static void build_remove_argtable(
    struct arg_lit **help,
    struct arg_str **name,
    struct arg_str **version,
    struct arg_end **end,
    void **tbl)          /* caller allocates: void *tbl[5] */
{
    *help    = arg_lit0("h", "help", "show this help and exit");
    *name    = arg_str1(NULL, NULL, "<name>",    "package name");
    *version = arg_str0(NULL, NULL, "[version]", "specific version to remove (default: latest)");
    *end     = arg_end(20);
    tbl[0] = *help; tbl[1] = *name; tbl[2] = *version; tbl[3] = *end; tbl[4] = NULL;
}

/* ── generic helpers ─────────────────────────────────────────────────────── */

static int run_command(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "pkg: exec %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return -1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static char *expand_home(const char *path, char *buf, size_t size)
{
    if (path[0] != '~') {
        if (strlen(path) >= size) return NULL;
        strcpy(buf, path);
        return buf;
    }
    const char *home = getenv("HOME");
    if (!home) return NULL;
    int n = snprintf(buf, size, "%s%s", home, path + 1);
    return (n > 0 && (size_t)n < size) ? buf : NULL;
}

static int mkdirs(const char *path)
{
    char tmp[4096];
    if ((size_t)snprintf(tmp, sizeof tmp, "%s", path) >= sizeof tmp) return -1;
    for (size_t i = 1; tmp[i]; i++) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
            fprintf(stderr, "pkg: mkdir %s: %s\n", tmp, strerror(errno));
            return -1;
        }
        tmp[i] = '/';
    }
    if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
        fprintf(stderr, "pkg: mkdir %s: %s\n", tmp, strerror(errno));
        return -1;
    }
    return 0;
}

static int find_first_subdir(const char *dir, char *out, size_t out_size)
{
    DIR *d = opendir(dir);
    if (!d) { perror("opendir"); return -1; }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        char full[4096];
        snprintf(full, sizeof full, "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(out, out_size, "%s", ent->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

/* ── minimal JSON helpers ────────────────────────────────────────────────── */

static int json_get_string(const char *json, const char *key,
                           char *out, size_t out_size)
{
    char needle[MAX_FIELD + 4];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':')
        p++;
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_size) {
        if (*p == '\\') { p++; if (!*p) break; }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

static int json_get_string_array(const char *json, const char *key,
                                 char bins[][MAX_BIN_LEN], int max)
{
    char needle[MAX_FIELD + 4];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':')
        p++;
    if (*p != '[') return 0;
    p++;
    int count = 0;
    while (*p && *p != ']' && count < max) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')
            p++;
        if (*p == '"') {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i + 1 < MAX_BIN_LEN) {
                if (*p == '\\') { p++; if (!*p) break; }
                bins[count][i++] = *p++;
            }
            bins[count][i] = '\0';
            if (*p == '"') p++;
            count++;
        } else if (*p != ']') {
            p++;
        }
    }
    return count;
}

/* ── subcommand helpers ──────────────────────────────────────────────────── */

static void print_subcmd_usage(FILE *out, const char *sub,
                               void **tbl, const char *desc)
{
    fprintf(out, "Usage: pkg %s ", sub);
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\n%s\n\nOptions:\n", desc);
    arg_print_glossary(out, tbl, "  %-26s %s\n");
}

/* ── subcommands ─────────────────────────────────────────────────────────── */

static int cmd_build(int argc, char **argv, FILE *out_stream)
{
    struct arg_lit *help;
    struct arg_str *src, *output;
    struct arg_end *end;
    void *tbl[5];
    build_build_argtable(&help, &src, &output, &end, tbl);

    /* Parse from argv+1 so argtable sees "build" as argv[0] */
    int nerrors = arg_parse(argc - 1, argv + 1, tbl);

    if (help->count > 0) {
        print_subcmd_usage(out_stream, "build", tbl,
                           "Pack a directory into a .tar.gz package archive.");
        arg_freetable(tbl, 4);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "pkg build");
        fprintf(stderr, "Try 'pkg build --help' for more information.\n");
        arg_freetable(tbl, 4);
        return 1;
    }

    const char *src_path    = src->sval[0];
    const char *output_path = output->sval[0];
    arg_freetable(tbl, 4);

    char src_copy[4096];
    if ((size_t)snprintf(src_copy, sizeof src_copy, "%s", src_path) >= sizeof src_copy) {
        fprintf(stderr, "pkg build: path too long\n");
        return 1;
    }

    char parent[4096] = ".";
    char base[4096];
    char *slash = strrchr(src_copy, '/');
    if (slash && slash != src_copy) {
        *slash = '\0';
        snprintf(parent, sizeof parent, "%s", src_copy);
        snprintf(base,   sizeof base,   "%s", slash + 1);
    } else if (slash == src_copy) {
        snprintf(parent, sizeof parent, "/");
        snprintf(base,   sizeof base,   "%s", src_copy + 1);
    } else {
        snprintf(base, sizeof base, "%s", src_copy);
    }

    char *tar_argv[] = { "tar", "-czf", (char *)output_path, "-C", parent, base, NULL };
    fprintf(out_stream, "pkg build: packing '%s' → %s\n", src_path, output_path);
    int rc = run_command(tar_argv);
    if (rc != 0)
        fprintf(stderr, "pkg build: tar exited with status %d\n", rc);
    return rc;
}

static int cmd_install(int argc, char **argv, FILE *out_stream)
{
    struct arg_lit *help;
    struct arg_str *archive;
    struct arg_end *end;
    void *tbl[4];
    build_install_argtable(&help, &archive, &end, tbl);

    int nerrors = arg_parse(argc - 1, argv + 1, tbl);

    if (help->count > 0) {
        print_subcmd_usage(out_stream, "install", tbl,
                           "Install a package archive under ~/.mysh/pkgs/ and\n"
                           "symlink its declared binaries into ~/.mysh/bin/.");
        arg_freetable(tbl, 3);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "pkg install");
        fprintf(stderr, "Try 'pkg install --help' for more information.\n");
        arg_freetable(tbl, 3);
        return 1;
    }

    const char *archive_path = archive->sval[0];
    arg_freetable(tbl, 3);

    /* Extract to scratch dir */
    char tmpdir[] = "/tmp/pkg-XXXXXX";
    if (!mkdtemp(tmpdir)) { perror("mkdtemp"); return 1; }

    char *untar[] = { "tar", "-xzf", (char *)archive_path, "-C", tmpdir, NULL };
    if (run_command(untar) != 0) {
        fprintf(stderr, "pkg install: extraction failed\n");
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }

    /* Locate the top-level dir the archive unpacked into */
    char subdir[256];
    if (find_first_subdir(tmpdir, subdir, sizeof subdir) < 0) {
        fprintf(stderr, "pkg install: no directory found in archive\n");
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }
    char extracted[4096];
    snprintf(extracted, sizeof extracted, "%s/%s", tmpdir, subdir);

    /* Parse pkg.json */
    char json_path[PATHBUF];
    snprintf(json_path, sizeof json_path, "%s/pkg.json", extracted);
    FILE *f = fopen(json_path, "r");
    if (!f) {
        fprintf(stderr, "pkg install: no pkg.json in archive (%s)\n", strerror(errno));
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long jsz = ftell(f);
    rewind(f);
    char *json = malloc((size_t)jsz + 1);
    if (!json) {
        fclose(f);
        fprintf(stderr, "pkg install: out of memory\n");
        return 1;
    }
    fread(json, 1, (size_t)jsz, f);
    json[jsz] = '\0';
    fclose(f);

    pkg_meta_t meta;
    memset(&meta, 0, sizeof meta);
    if (!json_get_string(json, "name", meta.name, sizeof meta.name)
            || meta.name[0] == '\0') {
        fprintf(stderr, "pkg install: pkg.json missing 'name'\n");
        free(json);
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }
    if (!json_get_string(json, "version", meta.version, sizeof meta.version)
            || meta.version[0] == '\0') {
        fprintf(stderr, "pkg install: pkg.json missing 'version'\n");
        free(json);
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }
    meta.nbin = json_get_string_array(json, "bin", meta.bins, MAX_BINS);
    free(json);

    /* Compute install paths */
    char pkgs_root[4096], install_dir[PATHBUF], bin_dir[4096];
    if (!expand_home("~/.mysh/pkgs", pkgs_root, sizeof pkgs_root) ||
        !expand_home("~/.mysh/bin",  bin_dir,   sizeof bin_dir)) {
        fprintf(stderr, "pkg install: HOME not set\n");
        return 1;
    }
    snprintf(install_dir, sizeof install_dir, "%s/%s-%s",
             pkgs_root, meta.name, meta.version);

    struct stat st;
    if (stat(install_dir, &st) == 0) {
        fprintf(stderr, "pkg install: %s-%s is already installed at %s\n",
                meta.name, meta.version, install_dir);
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }

    if (mkdirs(pkgs_root) < 0 || mkdirs(bin_dir) < 0) {
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }

    /* Move to final location; fall back to cp+rm across filesystems */
    if (rename(extracted, install_dir) < 0) {
        if (errno == EXDEV) {
            char *cp[] = { "cp", "-r", extracted, install_dir, NULL };
            if (run_command(cp) != 0) {
                fprintf(stderr, "pkg install: could not copy to %s\n", install_dir);
                char *rm[] = { "rm", "-rf", tmpdir, NULL };
                run_command(rm);
                return 1;
            }
        } else {
            fprintf(stderr, "pkg install: rename %s → %s: %s\n",
                    extracted, install_dir, strerror(errno));
            char *rm[] = { "rm", "-rf", tmpdir, NULL };
            run_command(rm);
            return 1;
        }
    }
    char *rm_tmp[] = { "rm", "-rf", tmpdir, NULL };
    run_command(rm_tmp);

    fprintf(out_stream, "pkg install: installed %s-%s\n           → %s\n",
            meta.name, meta.version, install_dir);

    /* Symlink binaries into ~/.mysh/bin/ */
    for (int i = 0; i < meta.nbin; i++) {
        char link_path[PATHBUF];
        char target[PATHBUF + MAX_BIN_LEN];
        snprintf(link_path, sizeof link_path, "%s/%s", bin_dir, meta.bins[i]);
        snprintf(target,    sizeof target,    "%s/%s", install_dir, meta.bins[i]);
        unlink(link_path);
        if (symlink(target, link_path) < 0)
            fprintf(stderr, "pkg install: symlink %s: %s\n",
                    meta.bins[i], strerror(errno));
        else
            fprintf(out_stream, "pkg install: linked  %s/%s\n", bin_dir, meta.bins[i]);
    }

    return 0;
}

static int cmd_list(int argc, char **argv, FILE *out_stream)
{
    struct arg_lit *help;
    struct arg_end *end;
    void *tbl[3];
    build_list_argtable(&help, &end, tbl);

    int nerrors = arg_parse(argc - 1, argv + 1, tbl);

    if (help->count > 0) {
        print_subcmd_usage(out_stream, "list", tbl,
                           "List all packages installed under ~/.mysh/pkgs/.");
        arg_freetable(tbl, 2);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "pkg list");
        fprintf(stderr, "Try 'pkg list --help' for more information.\n");
        arg_freetable(tbl, 2);
        return 1;
    }
    arg_freetable(tbl, 2);

    char pkgs_root[4096];
    if (!expand_home("~/.mysh/pkgs", pkgs_root, sizeof pkgs_root)) {
        fprintf(stderr, "pkg list: HOME not set\n");
        return 1;
    }

    DIR *d = opendir(pkgs_root);
    if (!d) {
        if (errno == ENOENT) {
            fprintf(out_stream, "No packages installed.\n");
            return 0;
        }
        fprintf(stderr, "pkg list: %s: %s\n", pkgs_root, strerror(errno));
        return 1;
    }

    int found = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char pkg_dir[PATHBUF];
        snprintf(pkg_dir, sizeof pkg_dir, "%s/%s", pkgs_root, ent->d_name);
        struct stat st;
        if (stat(pkg_dir, &st) < 0 || !S_ISDIR(st.st_mode)) continue;

        char json_path[PATHBUF + 16];
        snprintf(json_path, sizeof json_path, "%s/pkg.json", pkg_dir);
        char desc[256] = "";
        FILE *f = fopen(json_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long jsz = ftell(f);
            rewind(f);
            char *json = malloc((size_t)jsz + 1);
            if (json) {
                fread(json, 1, (size_t)jsz, f);
                json[jsz] = '\0';
                json_get_string(json, "description", desc, sizeof desc);
                free(json);
            }
            fclose(f);
        }

        if (desc[0])
            fprintf(out_stream, "  %-30s  %s\n", ent->d_name, desc);
        else
            fprintf(out_stream, "  %s\n", ent->d_name);
        found++;
    }
    closedir(d);

    if (!found)
        fprintf(out_stream, "No packages installed.\n");
    return 0;
}

static int cmd_remove(int argc, char **argv, FILE *out_stream)
{
    struct arg_lit *help;
    struct arg_str *name, *version;
    struct arg_end *end;
    void *tbl[5];
    build_remove_argtable(&help, &name, &version, &end, tbl);

    int nerrors = arg_parse(argc - 1, argv + 1, tbl);

    if (help->count > 0) {
        print_subcmd_usage(out_stream, "remove", tbl,
                           "Remove an installed package and its bin/ symlinks.");
        arg_freetable(tbl, 4);
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "pkg remove");
        fprintf(stderr, "Try 'pkg remove --help' for more information.\n");
        arg_freetable(tbl, 4);
        return 1;
    }

    const char *pkg_name = name->sval[0];
    const char *pkg_ver  = version->count > 0 ? version->sval[0] : NULL;
    arg_freetable(tbl, 4);

    char pkgs_root[4096], bin_dir[4096];
    if (!expand_home("~/.mysh/pkgs", pkgs_root, sizeof pkgs_root) ||
        !expand_home("~/.mysh/bin",  bin_dir,   sizeof bin_dir)) {
        fprintf(stderr, "pkg remove: HOME not set\n");
        return 1;
    }

    char install_dir[PATHBUF];

    if (pkg_ver) {
        snprintf(install_dir, sizeof install_dir, "%s/%s-%s",
                 pkgs_root, pkg_name, pkg_ver);
    } else {
        /* Scan for entries matching "<name>-*", pick lexicographically last */
        DIR *d = opendir(pkgs_root);
        if (!d) {
            fprintf(stderr, "pkg remove: %s: %s\n", pkgs_root, strerror(errno));
            return 1;
        }
        char prefix[MAX_FIELD + 2];
        snprintf(prefix, sizeof prefix, "%s-", pkg_name);
        size_t plen = strlen(prefix);

        char best[256] = "";
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strncmp(ent->d_name, prefix, plen) != 0) continue;
            char full[PATHBUF];
            snprintf(full, sizeof full, "%s/%s", pkgs_root, ent->d_name);
            struct stat st;
            if (stat(full, &st) < 0 || !S_ISDIR(st.st_mode)) continue;
            if (strcmp(ent->d_name, best) > 0)
                snprintf(best, sizeof best, "%s", ent->d_name);
        }
        closedir(d);

        if (best[0] == '\0') {
            fprintf(stderr, "pkg remove: '%s' is not installed\n", pkg_name);
            return 1;
        }
        snprintf(install_dir, sizeof install_dir, "%s/%s", pkgs_root, best);
    }

    struct stat st;
    if (stat(install_dir, &st) < 0) {
        fprintf(stderr, "pkg remove: %s: %s\n", install_dir, strerror(errno));
        return 1;
    }

    /* Remove bin/ symlinks that point into install_dir */
    DIR *bd = opendir(bin_dir);
    if (bd) {
        struct dirent *ent;
        while ((ent = readdir(bd)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char link_path[PATHBUF];
            snprintf(link_path, sizeof link_path, "%s/%s", bin_dir, ent->d_name);
            char target[PATHBUF + MAX_BIN_LEN];
            ssize_t n = readlink(link_path, target, sizeof target - 1);
            if (n < 0) continue;
            target[n] = '\0';
            if (strncmp(target, install_dir, strlen(install_dir)) == 0) {
                if (unlink(link_path) == 0)
                    fprintf(out_stream, "pkg remove: unlinked  %s\n", link_path);
            }
        }
        closedir(bd);
    }

    char *rm[] = { "rm", "-rf", install_dir, NULL };
    if (run_command(rm) != 0) {
        fprintf(stderr, "pkg remove: failed to remove %s\n", install_dir);
        return 1;
    }
    fprintf(out_stream, "pkg remove: removed %s\n", install_dir);
    return 0;
}

/* ── print_usage ─────────────────────────────────────────────────────────── */

void pkg_print_usage(FILE *out)
{
    fprintf(out, "Usage: pkg <subcommand> [options]\n\n");
    fprintf(out, "A local package manager for mysh extensions.\n");
    fprintf(out, "Packages install under ~/.mysh/pkgs/ with binaries symlinked\n");
    fprintf(out, "into ~/.mysh/bin/ (auto-added to PATH by mysh on startup).\n\n");
    fprintf(out, "Subcommands:\n");

    {
        struct arg_lit *help; struct arg_str *src, *output; struct arg_end *end;
        void *tbl[5];
        build_build_argtable(&help, &src, &output, &end, tbl);
        fprintf(out, "\n  pkg build ");
        arg_print_syntax(out, tbl, "\n");
        fprintf(out, "    Pack a directory into a .tar.gz package archive.\n");
        arg_freetable(tbl, 4);
    }
    {
        struct arg_lit *help; struct arg_str *archive; struct arg_end *end;
        void *tbl[4];
        build_install_argtable(&help, &archive, &end, tbl);
        fprintf(out, "\n  pkg install ");
        arg_print_syntax(out, tbl, "\n");
        fprintf(out, "    Install a package; symlink its binaries into ~/.mysh/bin/.\n");
        arg_freetable(tbl, 3);
    }
    {
        struct arg_lit *help; struct arg_end *end;
        void *tbl[3];
        build_list_argtable(&help, &end, tbl);
        fprintf(out, "\n  pkg list ");
        arg_print_syntax(out, tbl, "\n");
        fprintf(out, "    List installed packages with descriptions.\n");
        arg_freetable(tbl, 2);
    }
    {
        struct arg_lit *help; struct arg_str *name, *version; struct arg_end *end;
        void *tbl[5];
        build_remove_argtable(&help, &name, &version, &end, tbl);
        fprintf(out, "\n  pkg remove ");
        arg_print_syntax(out, tbl, "\n");
        fprintf(out, "    Remove a package and its bin/ symlinks.\n");
        arg_freetable(tbl, 4);
    }

    fprintf(out, "\nRun 'pkg <subcommand> --help' for subcommand-specific options.\n");
}

/* ── run ─────────────────────────────────────────────────────────────────── */

int pkg_run(int argc, char **argv, FILE *in_stream, FILE *out_stream)
{
    (void)in_stream;

    /* Top-level --help or no subcommand */
    if (argc < 2 ||
        strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "-h") == 0) {
        pkg_print_usage(out_stream);
        return argc < 2 ? 1 : 0;
    }

    const char *sub = argv[1];
    if (strcmp(sub, "build")   == 0) return cmd_build(argc, argv, out_stream);
    if (strcmp(sub, "install") == 0) return cmd_install(argc, argv, out_stream);
    if (strcmp(sub, "list")    == 0) return cmd_list(argc, argv, out_stream);
    if (strcmp(sub, "remove")  == 0) return cmd_remove(argc, argv, out_stream);

    fprintf(stderr, "pkg: unknown subcommand '%s'\n", sub);
    fprintf(stderr, "Run 'pkg --help' for available subcommands.\n");
    return 1;
}

/* ── cmd_spec_t ──────────────────────────────────────────────────────────── */

cmd_spec_t cmd_pkg_spec = {
    .name       = "pkg",
    .summary    = "local package manager",
    .long_help  = "Build, install, list, and remove mysh extension packages. "
                  "Packages are .tar.gz archives with a pkg.json manifest describing "
                  "the name, version, description, and binaries to symlink. "
                  "Run 'pkg --help' for subcommand details.",
    .run         = pkg_run,
    .print_usage = pkg_print_usage,
};
