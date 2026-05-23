# rahulbox — Design Document

## Overview

rahulbox is a custom Unix shell ecosystem built in C. It consists of:

1. **A set of reimplemented Unix utilities** (`ls`, `cat`, `wc`, `grep`, etc.), each built as both a standalone binary and a static library.
2. **`mysh`** — a small Unix shell that runs the utility commands in-process (no fork) and falls back to `fork` + `execvp` for everything else on `PATH`.
3. **`pkg`** — a local package manager for installing shell extensions.

The central design idea is the **Command Anatomy**: a shared `cmd_spec_t` struct that unifies argument parsing, help text generation, and shell registration across every command in the ecosystem.

---

## Repository layout

```
rahulbox/
├── include/
│   └── cmd_spec.h          # shared cmd_spec_t interface
├── vendor/
│   └── argtable3/          # vendored argument-parsing library
├── apps/
│   ├── hello/              # reference implementation
│   ├── ls/  wc/  cat/  stat/
│   ├── echo/  head/  tail/  grep/
│   ├── sort/  uniq/  cut/  tee/
│   └── pkg/
├── shell/
│   ├── mysh.c              # shell main loop + execution engine
│   ├── tok.c / tok.h       # tokenizer
│   └── Makefile
└── Makefile                # top-level coordinator
```

Each app directory contains exactly three files:
- `cmd_<name>.c` — all logic, exports a `cmd_spec_t`
- `<name>_main.c` — two-line entry point, calls `cmd_spec_t.run`
- `Makefile` — produces binary + static library

---

## The Command Anatomy

### `cmd_spec_t`

Defined in `include/cmd_spec.h`:

```c
typedef struct cmd_spec {
    const char *name;
    const char *summary;
    const char *long_help;
    int  (*run)(int argc, char **argv);
    void (*print_usage)(FILE *out);
} cmd_spec_t;
```

Every command module defines exactly one global instance of this struct. It is the only symbol the shell needs to import. `name` and `summary` populate the `help` listing; `long_help` is available for package documentation; `run` and `print_usage` implement the command.

### The argtable3 builder pattern

Each `cmd_<name>.c` contains a single static function `build_<name>_argtable()` that allocates all argtable3 argument objects and fills a `void *` table. Both `run()` and `print_usage()` call this same builder, so the CLI definition is never duplicated:

```c
static void build_cat_argtable(
    struct arg_lit  **help,
    struct arg_lit  **number,
    struct arg_lit  **show_ends,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *number    = arg_lit0("n", "number",    "number all output lines");
    *show_ends = arg_lit0("E", "show-ends", "display $ at end of each line");
    *json      = arg_lit0(NULL, "json",     "emit machine-readable JSON");
    *files     = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to concatenate");
    *end       = arg_end(20);

    static void *tbl[7];
    tbl[0] = *help; tbl[1] = *number; tbl[2] = *show_ends;
    tbl[3] = *json; tbl[4] = *files;  tbl[5] = *end; tbl[6] = NULL;
    *argtable_out = tbl;
}
```

**Why a static table?** argtable3 requires a `void **` array terminated by a NULL sentinel. Declaring it `static` inside the builder avoids heap allocation while keeping the pointer valid for the lifetime of the argtable. Only one invocation of the builder is live at a time (single-threaded), so the static is safe.

`print_usage` calls the builder, calls `arg_print_syntax` and `arg_print_glossary`, then frees. `run` calls the builder, parses, dispatches, then frees. The argtable is always freed before returning, even on error paths.

### `run()` call convention

Every `run()` function follows this sequence:

1. Build argtable
2. `arg_parse(argc, argv, argtable)`
3. Check `help->count > 0` first — print usage and return 0
4. Check `nerrors > 0` — print errors to stderr, return 1
5. Extract flag values via `->count` and `->ival[0]` / `->sval[0]`
6. Execute core logic
7. `arg_freetable(argtable, N)` before every return path

The main entry point (`<name>_main.c`) is always two lines:

```c
int main(int argc, char **argv) {
    return cmd_<name>_spec.run(argc, argv);
}
```

### `--json` convention

Every command implements `--json` for agent and MCP integration. The convention:

- Commands that process files emit a JSON array, one object per file.
- Single-input commands (e.g., `echo`, `tee`) emit a single JSON object.
- String values in JSON have newlines escaped as `\n` etc., via a local `json_escape()` helper present in each `cmd_*.c` (no shared dependency).
- `grep --json` emits `[{"file": "...", "matches": [{"line": N, "text": "..."}]}]`
- `uniq --json` emits `[{"count": N, "line": "..."}]`
- `sort --json` emits a flat JSON array of strings

---

## Build system

### Per-app Makefile

Each app's Makefile follows the same template:

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =
ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

all: <name> lib<name>.a

<name>: <name>_main.o cmd_<name>.o argtable3.o
    $(CC) $(LDFLAGS) -o $@ $^ -lm

lib<name>.a: cmd_<name>.o argtable3.o
    ar rcs $@ $^
```

Key decisions:
- **argtable3 is compiled per-app** from source. This avoids any system dependency and keeps each app fully self-contained.
- **Each app produces both a binary and a `.a` library.** The binary is used standalone; the `.a` is available for linking (though the shell uses the `.o` directly — see below).
- **`-lm` is always passed** because argtable3's hashtable uses `ceil()`.

### Shell Makefile

The shell links `cmd_<name>.o` files directly rather than the `.a` libraries:

```makefile
APP_OBJS = ../apps/cat/cmd_cat.o \
           ../apps/grep/cmd_grep.o \
           ...

mysh: mysh.o tok.o argtable3.o $(APP_OBJS)
    $(CC) -o $@ $^ -lm
```

**Why link `.o` instead of `.a`?** If the shell linked multiple `.a` files, each would carry its own copy of `argtable3.o`, causing duplicate symbol errors at link time. Linking the `cmd_*.o` files directly with a single shared `argtable3.o` avoids this entirely.

The shell's `apps` target runs `$(MAKE) -C ../apps/<name>` for each app before linking, ensuring app object files are up to date.

### Top-level Makefile

The root Makefile lists only the original five apps explicitly (it predates the later additions). `make` at the root builds those five plus the shell. The newer apps are built transitively when the shell's `apps` target runs.

---

## The shell (`mysh`)

### Architecture

`mysh` is a single-file shell (`mysh.c`, ~475 lines). Its main loop:

1. Print prompt if interactive
2. `fgets` one line
3. Tokenize via `tok_split`
4. Parse pipeline stages via `parse_pipeline`
5. Handle built-ins (`cd`, `exit`, `help`) for single-stage commands
6. Execute via `run_pipeline`

### Tokenizer (`tok.c`)

`tok_split` is a hand-written state machine that recognises:

- **Whitespace word boundaries** — consecutive whitespace collapses
- **Single quoting** `'...'` — contents taken literally, no escapes
- **Double quoting** `"..."` — `\"`, `\\`, `\$` are unescaped; other characters taken literally (no variable expansion)
- **Operators** `<`, `>`, `>>`, `|` — always emitted as their own tokens, even adjacent to words (e.g. `foo>bar` produces three tokens)
- **Comments** — `#` outside a word or quote starts a comment; rest of line is discarded

Result is a `tok_t` struct containing up to `TOK_MAX` (128) heap-allocated word strings. The caller frees with `tok_free()`.

### Pipeline parsing

`parse_pipeline` walks the token list and fills an array of `stage_t` structs (up to `PIPELINE_MAX = 16`). Each stage holds:

```c
typedef struct {
    char *argv[TOK_MAX];
    int   argc;
    char *redir_in;    /* < filename, or NULL */
    char *redir_out;   /* > or >> filename, or NULL */
    int   append;      /* 1 if >> */
} stage_t;
```

`|` tokens start a new stage. `<`, `>`, `>>` consume the next token as a filename. Everything else is an argument.

### Execution

**Single-stage pipeline — `run_inproc`:**

For a single command with no pipe, `run_inproc` is called:

1. Flush stdout/stderr to prevent buffered output from landing in the wrong file if redirection is about to happen.
2. If `redir_in` or `redir_out` is set, `dup` the original fd, `open` the file, `dup2` it to fd 0 or 1. The saved fd is restored after the command returns.
3. Look up the command in the registry with `reg_find`. If found, call `spec->run()` directly — no fork, no exec.
4. If not in registry, `fork` + `execvp`. The parent `waitpid`s.
5. Restore saved fds.

The in-process path is the key performance feature: `ls`, `cat`, `grep`, `sort`, etc. run without spawning a process.

**Multi-stage pipeline — `run_pipeline`:**

For two or more stages, every stage runs in a forked child:

```
stage[0] --> pipe[0] --> stage[1] --> pipe[1] --> stage[2] --> stdout
```

Each child:
1. Closes `g_cmd_fd` (the fd holding the shell's script/stdin) so the child doesn't accidentally inherit command text in fd 0.
2. `dup2`s `prev_rd` to stdin and `wr_fd` to stdout.
3. Closes unused pipe ends.
4. Applies file redirections on top.
5. Looks up the registry; if found, calls `spec->run()` then `_exit`. If not, `execvp`.

The parent closes each pipe end as it advances through the stages, then `waitpid`s all children and returns the exit status of the last stage.

**Why fork even registered commands in pipelines?**

Registered commands read from stdin and write to stdout. In a pipeline, stdin/stdout must be wired to pipes. Doing this in-process would require saving and restoring fd 0/1, and — critically — any buffered I/O state (`FILE *stdin`) would be corrupted for the next command. Forking gives each stage an isolated fd namespace at the cost of one extra process per stage.

### `g_cmd_fd` — the stdin contamination fix

When `mysh` is invoked with stdin as a pipe or redirect (not a tty and not a named script file), the shell reads commands from that pipe. If it used fd 0 directly, forked children would inherit the same fd and might read command text meant for the shell.

The fix: at startup, `dup(STDIN_FILENO)` to `g_cmd_fd` and read commands from `fdopen(g_cmd_fd, "r")`. Fd 0 is left untouched. Every fork child immediately calls `close(g_cmd_fd)` before doing anything else, so the shell retains sole ownership of the command stream.

### Signal handling

- The shell process ignores `SIGINT` and `SIGPIPE`.
- Forked children restore `SIGINT` and `SIGPIPE` to `SIG_DFL` before executing. This means Ctrl-C kills foreground children but not the shell itself, and broken pipes in pipeline children terminate normally.

### Built-in commands

Three built-ins must run in the shell process itself (not forkable):

- **`cd [DIR]`** — calls `chdir`. Updates `OLDPWD` before changing; `cd -` reads `OLDPWD` and prints the destination (matching bash behaviour). `cd` with no argument uses `$HOME`.
- **`exit [N]`** — exits with `N`, or with `last_status` if no argument.
- **`help`** — calls `reg_print_all()` to list all registered commands with their summaries, plus the built-in list.

### PATH integration

At startup, `setup_path` checks whether `~/.mysh/bin` exists. If so, it prepends it to `PATH` (avoiding double-prepending on nested shell invocations). This makes packages installed by `pkg install` immediately available as commands.

### Registry

The registry is a fixed-size flat array of `cmd_spec_t *` (max 64 entries). `reg_register` appends; `reg_find` is a linear scan by name. At 13 current commands this is negligible cost. Registration order determines the order entries appear in `help` output.

---

## App implementations

### `hello` — reference implementation

The simplest possible command: one string option (`--name`), one output line. Its `apps/hello/` directory also includes `registry.c` — a standalone version of the registry pattern (`registry_register`, `registry_find`, `registry_print_all`) used in early exploration before the registry was folded into `mysh.c`.

### `ls` — list directory contents

Uses `opendir`/`readdir`/`closedir` (POSIX). Calls `lstat` on each entry to get file size and type. Flags: `-a` (include dotfiles). JSON output: one object per directory path, with an `entries` array of `{name, type, size}` objects. Multiple path arguments produce a JSON array.

### `stat` — display file status

Calls `lstat` on each file. Formats permissions as a symbolic string (e.g. `-rw-rw-r--`) and as octal. Resolves uid/gid to names via `getpwuid`/`getgrgid`. Formats timestamps with `strftime`. The key design decision: uses `lstat` not `stat`, so symlinks are reported as symlinks.

### `wc` — word/line/byte counter

Reads line by line with `getline`. Counts newlines for `-l`, splits on whitespace runs for `-w`, accumulates byte count for `-c`, uses `mblen` with `LC_ALL` locale for `-m` (character count). When no mode flag is given, defaults to all three. Multiple files produce a totals row.

### `cat` — concatenate files

Reads with `getline` for `-n`/`-E` modes (line-oriented), with `fread` for JSON mode (reads entire file into a buffer for clean JSON escaping). `-n` numbers lines; `-E` strips trailing newline, appends `$`, then newlines. `-` as filename reads stdin.

### `echo` — display text

Implements `-n` (suppress trailing newline) and `-e` (backslash escapes: `\n`, `\t`, `\r`, `\\`, `\0`, `\a`, `\b`, `\f`, `\v`). JSON mode emits `{"output": "..."}`. The `-e` escape processor walks the string looking for `\` followed by a recognized letter.

### `head` — first N lines/bytes

Two core functions: `head_lines` (uses `getline`, counts) and `head_bytes` (uses `fread` with a remaining-bytes counter). Multiple files get `==> file <==` headers (matching GNU head). JSON mode buffers lines into memory then emits as an escaped string.

### `tail` — last N lines/bytes

For lines: reads all lines from the stream into a heap-allocated `char **` array (same `read_all_lines` helper as `sort`), then prints starting from `max(0, count - n)`. For bytes: reads entire stream, then writes from `size - n`. Both approaches require buffering the full input — simple and correct for files of reasonable size.

### `grep` — pattern search

Compiles `PATTERN` as a POSIX extended regular expression with `regcomp(REG_EXTENDED)`. Adding `-i` passes `REG_ICASE`. Each line is tested with `regexec`. Flags: `-n` (line numbers), `-c` (count per file), `-v` (invert), `-l` (files-with-matches). Exit codes follow the grep convention: 0 = match found, 1 = no match, 2 = error. JSON output nests matches inside a per-file object.

### `sort` — sort lines

Reads all lines from all inputs into a single heap-allocated `char **` array, then calls `qsort` with a custom comparator. Comparator state is held in four module-level globals (`g_reverse`, `g_numeric`, `g_key`, `g_sep`) — safe because `qsort` is single-threaded. Field extraction for `-k` splits on whitespace (default) or on the `-t` separator character.

`-u` (unique): after sorting, a second pass marks adjacent equal lines `NULL` (freeing the string); the output pass skips NULLs. This avoids an auxiliary data structure.

### `uniq` — filter adjacent duplicates

Streams line-by-line, grouping adjacent equal lines into `entry_t` records `{text, len, hits}`. Does not require input to be sorted — only adjacent duplicates are collapsed, matching standard `uniq` behaviour. Pair with `sort | uniq` for global deduplication. Accepts optional positional INPUT and OUTPUT filenames.

### `cut` — field/character selection

Parses the LIST specification (`"1,3-5,7"`) into a `char selected[4096]` boolean array (1-based, index 0 unused) using `strtol` with range expansion. `-f` mode splits each line on the delimiter character (default TAB) and outputs selected fields re-joined by the same delimiter. `-c` mode selects byte positions directly. The two modes cannot be combined.

The 4096-element boolean array keeps the selection logic to a simple indexed lookup per field/character — no sorted-interval tree needed for the field counts typical in practice.

### `tee` — fanout stdin to multiple files

Opens all output files before reading. Reads stdin in 4 KiB chunks and `fwrite`s each chunk to stdout and every open file in the same loop iteration. This gives true simultaneous fanout with no extra buffering. JSON mode buffers the entire input first (to report `bytes_read`), then writes to files in the same pass.

### `pkg` — local package manager

Unlike the other apps, `pkg` does not use the `cmd_spec_t` anatomy or `argtable3`. It is a self-contained single-file program with manual `argv` dispatch. Subcommands: `build` (shells out to `tar`), `install` (shells out to `tar -xz` into `~/.mysh/pkgs/<name>-<ver>/`, then symlinks declared binaries into `~/.mysh/bin/`), `list` (scans `~/.mysh/pkgs/`), `remove` (unlinks bin symlinks, then `rm -rf`). Packages are described by a `pkg.json` manifest parsed with a minimal hand-written JSON reader (no library dependency).

---

## Key design decisions

### argtable3 as the single source of truth

Every flag, its short name, its long name, its metavar, and its help string is declared exactly once — in `build_<name>_argtable()`. `print_usage` and `run` both call this builder. This means `--help` output is always consistent with what the parser actually accepts, and adding a new flag in one place automatically updates both.

### No shared helpers between apps

Each `cmd_*.c` defines its own `json_escape()` helper rather than sharing one from a common header. This is deliberate: it keeps each app a self-contained compilation unit with no dependency on sibling code, making it easy to lift an app out of the tree and use it elsewhere. The duplication (~15 lines per file) is a reasonable price for zero coupling.

### Dual-output binary + library

Every app builds to both `<name>` (standalone binary) and `lib<name>.a` (static library). The standalone binary is useful directly; the library is available if a future host program wants to embed a command. The shell uses neither — it links `cmd_<name>.o` directly to avoid duplicate `argtable3` symbols.

### In-process execution for registered commands

Registered commands run in-process for single-stage invocations: no `fork`, no `exec`, no process creation overhead. For a shell used interactively with many short commands, this is a meaningful latency improvement. The tradeoff is that a misbehaving command (segfault, infinite loop) crashes the shell — acceptable in a course/educational context.

In pipelines, every stage forks regardless, because pipes require fd rewiring that cannot be safely done in-process with `FILE *`-based I/O.

### stdin contamination fix (`g_cmd_fd`)

When the shell's own command input arrives on fd 0 (e.g. `echo "ls\nexit" | mysh`), forked children would inherit that fd and could accidentally consume command text. The fix — `dup`ing fd 0 to a new fd and reading commands from there — ensures fd 0 remains clean for children. Each fork child immediately closes `g_cmd_fd`.

### `_POSIX_C_SOURCE 200809L` and `_GNU_SOURCE`

The shell uses `_POSIX_C_SOURCE 200809L` for `getline`, `dprintf`, and other POSIX.1-2008 extensions. App modules use `_GNU_SOURCE` (a superset) which additionally unlocks `strcasestr`, `strndup`, `getline`, and other GNU extensions. Both are Linux-only, consistent with the stated target platform.

### Fixed-size limits

| Constant | Value | Used for |
|---|---|---|
| `TOK_MAX` | 128 | Max tokens per input line |
| `PIPELINE_MAX` | 16 | Max pipe stages |
| `REGISTRY_MAX` | 64 | Max registered commands |
| `MAX_POSITIONS` | 4096 | Max field/char index in `cut` |

These are generous enough for all real use cases and avoid the complexity of dynamic resizing in the shell's hot path.
