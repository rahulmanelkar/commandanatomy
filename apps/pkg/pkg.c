/*
 * pkg — package manager for mysh
 *
 * Subcommands:
 *   build   <src-dir> <output.tar.gz>   pack a directory into a package
 *   install <pkg.tar.gz>                install a package under ~/.mysh
 *   list                                list installed packages
 *   remove  <name>                      remove an installed package
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* ── constants ──────────────────────────────────────────────────────────── */

#define MAX_BINS      32
#define MAX_BIN_LEN  256
#define MAX_FIELD    128
/* Buffers that hold a full path plus appended components must be larger. */
#define PATHBUF     8192

/* ── types ──────────────────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    int (*fn)(int argc, char **argv);
    const char *usage;
} subcmd_t;

typedef struct {
    char name[MAX_FIELD];
    char version[MAX_FIELD];
    char bins[MAX_BINS][MAX_BIN_LEN];
    int  nbin;
} pkg_meta_t;

/* ── forward declarations ───────────────────────────────────────────────── */

static int cmd_build(int argc, char **argv);
static int cmd_install(int argc, char **argv);
static int cmd_list(int argc, char **argv);
static int cmd_remove(int argc, char **argv);

/* ── dispatch table ─────────────────────────────────────────────────────── */

static const subcmd_t commands[] = {
    { "build",   cmd_build,   "pkg build <src-dir> <output.tar.gz>" },
    { "install", cmd_install, "pkg install <pkg.tar.gz>"            },
    { "list",    cmd_list,    "pkg list"                            },
    { "remove",  cmd_remove,  "pkg remove <name>"                   },
};
static const int NUM_COMMANDS = (int)(sizeof commands / sizeof *commands);

/* ── generic helpers ────────────────────────────────────────────────────── */

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

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <subcommand> [args]\n\n", prog);
    fprintf(stderr, "Subcommands:\n");
    for (int i = 0; i < NUM_COMMANDS; i++)
        fprintf(stderr, "  %s\n", commands[i].usage);
}

/* ── path helpers ───────────────────────────────────────────────────────── */

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

/* mkdir -p: creates every component in path, ignores EEXIST. */
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

/*
 * Find the first subdirectory inside dir and write its name (not full path)
 * into out.  Returns 0 on success, -1 if none found.
 */
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

/* ── minimal JSON helpers ───────────────────────────────────────────────── */

/*
 * Extract the first string value for key from a flat JSON object.
 * Handles basic backslash escapes.  Returns 1 on success, 0 if not found.
 */
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

/*
 * Extract an array of strings for key from a flat JSON object.
 * Fills bins[0..max-1][MAX_BIN_LEN].  Returns the number of entries found.
 */
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

/* ── subcommands ────────────────────────────────────────────────────────── */

static int cmd_build(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s\n", commands[0].usage);
        return 1;
    }
    const char *src    = argv[2];
    const char *output = argv[3];

    char src_copy[4096];
    if ((size_t)snprintf(src_copy, sizeof src_copy, "%s", src) >= sizeof src_copy) {
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

    char *tar_argv[] = { "tar", "-czf", (char *)output, "-C", parent, base, NULL };
    printf("pkg build: packing '%s' → %s\n", src, output);
    int rc = run_command(tar_argv);
    if (rc != 0)
        fprintf(stderr, "pkg build: tar exited with status %d\n", rc);
    return rc;
}

/*
 * pkg install <archive.tar.gz>
 *
 * 1. Extract archive to a mkdtemp scratch directory.
 * 2. Read and parse the top-level pkg.json (name, version, bin[]).
 * 3. Move the extracted tree to ~/.mysh/pkgs/<name>-<version>/.
 * 4. Symlink each listed binary into ~/.mysh/bin/.
 */
static int cmd_install(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s\n", commands[1].usage);
        return 1;
    }
    const char *archive = argv[2];

    /* ── step 1: extract to scratch dir ─────────────────────────────────── */

    char tmpdir[] = "/tmp/pkg-XXXXXX";
    if (!mkdtemp(tmpdir)) { perror("mkdtemp"); return 1; }

    char *untar[] = { "tar", "-xzf", (char *)archive, "-C", tmpdir, NULL };
    if (run_command(untar) != 0) {
        fprintf(stderr, "pkg install: extraction failed\n");
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }

    /* ── step 2: locate the top-level dir the archive unpacked into ──────── */

    char subdir[256];
    if (find_first_subdir(tmpdir, subdir, sizeof subdir) < 0) {
        fprintf(stderr, "pkg install: no directory found in archive\n");
        char *rm[] = { "rm", "-rf", tmpdir, NULL };
        run_command(rm);
        return 1;
    }
    char extracted[4096];
    snprintf(extracted, sizeof extracted, "%s/%s", tmpdir, subdir);

    /* ── step 3: parse pkg.json ──────────────────────────────────────────── */

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

    /* ── step 4: compute install paths ──────────────────────────────────── */

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

    /* ── step 5: move extracted tree to final location ───────────────────── */

    if (rename(extracted, install_dir) < 0) {
        /*
         * rename(2) fails with EXDEV when src and dest are on different
         * filesystems (e.g. /tmp is a tmpfs).  Fall back to cp + rm.
         */
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

    printf("pkg install: installed %s-%s\n           → %s\n",
           meta.name, meta.version, install_dir);

    /* ── step 6: symlink binaries into ~/.mysh/bin/ ──────────────────────── */

    for (int i = 0; i < meta.nbin; i++) {
        /* install_dir < 4096 + 2*MAX_FIELD in practice; target fits in PATHBUF */
        char link_path[PATHBUF];
        char target[PATHBUF + MAX_BIN_LEN];
        snprintf(link_path, sizeof link_path, "%s/%s", bin_dir, meta.bins[i]);
        snprintf(target,    sizeof target,    "%s/%s", install_dir, meta.bins[i]);

        unlink(link_path);   /* remove stale symlink if present */
        if (symlink(target, link_path) < 0)
            fprintf(stderr, "pkg install: symlink %s: %s\n",
                    meta.bins[i], strerror(errno));
        else
            printf("pkg install: linked  %s/%s\n", bin_dir, meta.bins[i]);
    }

    return 0;
}

/*
 * pkg list
 *
 * Enumerate ~/.mysh/pkgs/ — each subdirectory is an installed package.
 * Read its pkg.json to show name, version, and description if available.
 */
static int cmd_list(int argc, char **argv)
{
    (void)argc; (void)argv;

    char pkgs_root[4096];
    if (!expand_home("~/.mysh/pkgs", pkgs_root, sizeof pkgs_root)) {
        fprintf(stderr, "pkg list: HOME not set\n");
        return 1;
    }

    DIR *d = opendir(pkgs_root);
    if (!d) {
        if (errno == ENOENT) {
            printf("No packages installed.\n");
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

        /* Try to read pkg.json for description */
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
            printf("  %-30s  %s\n", ent->d_name, desc);
        else
            printf("  %s\n", ent->d_name);
        found++;
    }
    closedir(d);

    if (!found)
        printf("No packages installed.\n");
    return 0;
}

/*
 * pkg remove <name>           — remove newest version
 * pkg remove <name> <version> — remove specific version
 *
 * Deletes ~/.mysh/pkgs/<name>-<version>/ and unlinks any ~/.mysh/bin/
 * symlinks that resolve into that directory.
 */
static int cmd_remove(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s\n", commands[3].usage);
        return 1;
    }

    char pkgs_root[4096], bin_dir[4096];
    if (!expand_home("~/.mysh/pkgs", pkgs_root, sizeof pkgs_root) ||
        !expand_home("~/.mysh/bin",  bin_dir,   sizeof bin_dir)) {
        fprintf(stderr, "pkg remove: HOME not set\n");
        return 1;
    }

    const char *name = argv[2];
    char install_dir[PATHBUF];

    if (argc >= 4) {
        /* explicit version */
        snprintf(install_dir, sizeof install_dir, "%s/%s-%s",
                 pkgs_root, name, argv[3]);
    } else {
        /*
         * No version given — scan pkgs_root for entries matching "<name>-*"
         * and pick the lexicographically last one (good enough for semver).
         */
        DIR *d = opendir(pkgs_root);
        if (!d) {
            fprintf(stderr, "pkg remove: %s: %s\n", pkgs_root, strerror(errno));
            return 1;
        }
        char prefix[MAX_FIELD + 2];
        snprintf(prefix, sizeof prefix, "%s-", name);
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
            fprintf(stderr, "pkg remove: '%s' is not installed\n", name);
            return 1;
        }
        snprintf(install_dir, sizeof install_dir, "%s/%s", pkgs_root, best);
    }

    /* Verify the directory exists */
    struct stat st;
    if (stat(install_dir, &st) < 0) {
        fprintf(stderr, "pkg remove: %s: %s\n", install_dir, strerror(errno));
        return 1;
    }

    /* Remove any bin/ symlinks that point into install_dir */
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
                    printf("pkg remove: unlinked  %s\n", link_path);
            }
        }
        closedir(bd);
    }

    /* Delete the package directory tree */
    char *rm[] = { "rm", "-rf", install_dir, NULL };
    if (run_command(rm) != 0) {
        fprintf(stderr, "pkg remove: failed to remove %s\n", install_dir);
        return 1;
    }
    printf("pkg remove: removed %s\n", install_dir);
    return 0;
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc < 2) { usage(argv[0]); return 1; }
    const char *sub = argv[1];
    for (int i = 0; i < NUM_COMMANDS; i++)
        if (strcmp(sub, commands[i].name) == 0)
            return commands[i].fn(argc, argv);
    fprintf(stderr, "pkg: unknown subcommand '%s'\n", sub);
    usage(argv[0]);
    return 1;
}
