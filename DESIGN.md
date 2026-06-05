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
│   ├── pkg/               # local package manager
│   └── fetch/             # TCP line client
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
    int  (*run)(int argc, char **argv, FILE *in_stream, FILE *out_stream);
    void (*print_usage)(FILE *out);
} cmd_spec_t;
```

Every command module defines exactly one global instance of this struct. It is the only symbol the shell needs to import. `name` and `summary` populate the `help` listing; `long_help` is available for package documentation; `run` and `print_usage` implement the command.

### The argtable3 builder pattern

Each `cmd_<name>.c` contains a single static function `build_<name>_argtable()` that allocates all argtable3 argument objects and fills a caller-supplied `void *` table. Both `run()` and `print_usage()` call this same builder, so the CLI definition is never duplicated:

```c
static void build_cat_argtable(
    struct arg_lit  **help,
    struct arg_lit  **number,
    struct arg_lit  **show_ends,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void            **tbl)          /* caller allocates: void *tbl[7] */
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *number    = arg_lit0("n", "number",    "number all output lines");
    *show_ends = arg_lit0("E", "show-ends", "display $ at end of each line");
    *json      = arg_lit0(NULL, "json",     "emit machine-readable JSON");
    *files     = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to concatenate");
    *end       = arg_end(20);

    tbl[0] = *help; tbl[1] = *number; tbl[2] = *show_ends;
    tbl[3] = *json; tbl[4] = *files;  tbl[5] = *end; tbl[6] = NULL;
}
```

The caller declares `void *tbl[N]` on its own stack frame and passes it in. This makes the builder thread-safe: each concurrent invocation operates on its own stack-local table with no shared mutable state. The table is valid for the duration of the calling function and is freed via `arg_freetable(tbl, N)` before every return path.

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
    return cmd_<name>_spec.run(argc, argv, stdin, stdout);
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
- `fetch --json` emits a single object with an `ok` discriminator — `{"ok": true, "host": ..., "port": ..., "sent": ..., "bytes": N, "reply": ...}` on success, `{"ok": false, "host": ..., "port": ..., "error": ...}` on failure. Unlike the file commands, `fetch` writes its error object to stdout (not stderr) in `--json` mode, so an agent capturing stdout always receives a parseable result.

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
    $(CC) -o $@ $^ -lm -lpthread
```

**Why link `.o` instead of `.a`?** If the shell linked multiple `.a` files, each would carry its own copy of `argtable3.o`, causing duplicate symbol errors at link time. Linking the `cmd_*.o` files directly with a single shared `argtable3.o` avoids this entirely.

The shell's `apps` target runs `$(MAKE) -C ../apps/<name>` for each app before linking, ensuring app object files are up to date.

### Top-level Makefile

The root Makefile lists only the original five apps explicitly (it predates the later additions). `make` at the root builds those five plus the shell. The newer apps are built transitively when the shell's `apps` target runs.

---

## The shell (`mysh`)

### Architecture

`mysh` is a single-file shell (`mysh.c`, ~530 lines). Its main loop:

1. Reap finished background jobs with `waitpid(-1, NULL, WNOHANG)`
2. Print prompt if interactive
3. `fgets` one line
4. Tokenize via `tok_split` (expands `$VAR`/`${VAR}`/`$?` in place)
5. Strip a trailing `&` token and set a `bg` flag if present
6. Parse pipeline stages via `parse_pipeline`
7. Handle built-ins (`cd`, `exit`, `help`, `VAR=value`) for single-stage commands
8. Execute via `run_pipeline`; if `bg`, fork the whole pipeline and continue

### Tokenizer (`tok.c`)

`tok_split(line, out, last_status)` is a hand-written state machine that recognises:

- **Whitespace word boundaries** — consecutive whitespace collapses
- **Single quoting** `'...'` — contents taken literally, no escapes, no variable expansion
- **Double quoting** `"..."` — `\"`, `\\`, `\$` are unescaped; `$VAR`/`${VAR}`/`$?` are expanded; other characters taken literally
- **Variable expansion** `$VAR`, `${VAR}`, `$?` — resolved via `getenv()` in both normal and double-quote modes; `$?` uses the `last_status` parameter; an unrecognised bare `$` is emitted literally
- **Operators** `<`, `>`, `>>`, `|`, `&` — always emitted as their own tokens, even adjacent to words
- **Comments** — `#` outside a word or quote starts a comment; rest of line is discarded

Result is a `tok_t` struct containing up to `TOK_MAX` (128) heap-allocated word strings, with all variable references already expanded. The caller frees with `tok_free()`.

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

1. Look up the command in the registry with `reg_find`.
2. If found, resolve file redirections by `fopen`ing the named files into local `FILE *in` / `FILE *out` variables (falling back to `stdin`/`stdout` when no redirection is specified), then call `spec->run(argc, argv, in, out)` directly — no fork, no exec, no `dup2`.
3. If not in registry, `fork` + `execvp` with `dup2`-based redirection in the child. The parent `waitpid`s.

The in-process path is the key performance feature: `ls`, `cat`, `grep`, `sort`, etc. run without spawning a process.

**Multi-stage pipeline — `run_pipeline`:**

For two or more stages, internal (registered) commands run as POSIX threads; external commands still fork:

```
stage[0]──thread──┐pipe[0]┌──thread──stage[1]──┐pipe[1]┌──thread──stage[2]
                  └───────┘                     └───────┘
```

For each stage in order:

1. Create a Unix pipe with `pipe()`. Both ends are immediately marked `FD_CLOEXEC` so they are not inherited by any `fork`'d external-command child in the same pipeline.
2. Determine the input source for this stage:
   - If it is the first stage and there is a `redir_in`, `fopen` that file; otherwise `fdopen(dup(STDIN_FILENO), "r")` to get a `FILE*` the thread can safely `fclose` without closing the shell's real fd 0.
   - Otherwise `fdopen` the read end of the previous stage's pipe.
3. Determine the output sink:
   - If it is the last stage and there is a `redir_out`, `fopen` that file; otherwise `fdopen(dup(STDOUT_FILENO), "w")` for the same reason.
   - Otherwise `fdopen` the write end of the new pipe.
4. **Internal command:** allocate a `stage_arg_t` on the heap:
   ```c
   typedef struct {
       cmd_spec_t *spec;
       int         argc;
       char      **argv;
       FILE       *in;
       FILE       *out;
       int         exit_code;
   } stage_arg_t;
   ```
   Spawn a thread with `pthread_create` running `stage_thread_fn`, which calls `spec->run(argc, argv, in, out)`, then `fclose(out)` and `fclose(in)`. Closing `out` signals EOF to the downstream reader. Store the `pthread_t` and `stage_arg_t *` in a `spawn_t` record (`is_thread = 1`).
5. **External command:** `fork`. The child clears `FD_CLOEXEC` on the assigned pipe fds, `dup2`s them to fd 0/1, closes all pipe ends, closes `g_cmd_fd`, and `execvp`s. The parent closes the raw pipe fds it just handed to the child and stores the `pid_t` in a `spawn_t` record (`is_thread = 0`).

After all stages are spawned, the shell collects results:
- `pthread_join` for every thread spawn; the `stage_arg_t.exit_code` field holds the return value of `run()`.
- `waitpid` for every process spawn.
- The exit status of the last stage is returned as the pipeline result.

`FD_CLOEXEC` on all pipe fds ensures that no forked external-command child accidentally holds a write end of an inter-thread pipe open (which would prevent EOF from being delivered to the downstream thread).

### `g_cmd_fd` — the stdin contamination fix

When `mysh` is invoked with stdin as a pipe or redirect (not a tty and not a named script file), the shell reads commands from that pipe. If it used fd 0 directly, forked children would inherit the same fd and might read command text meant for the shell.

The fix: at startup, `dup(STDIN_FILENO)` to `g_cmd_fd` and read commands from `fdopen(g_cmd_fd, "r")`. Fd 0 is left untouched. Every fork child immediately calls `close(g_cmd_fd)` before doing anything else, so the shell retains sole ownership of the command stream.

### Background execution

When a command line ends with `&`, the shell forks the entire `run_pipeline` call into a background child process, prints `[bg] PID`, sets `last_status = 0`, and immediately returns to the prompt. The background child runs the pipeline (whether internal threads or forked external commands) synchronously and then exits.

Background children inherit `SIGINT = SIG_IGN` from the shell (appropriate for non-interactive background jobs). The shell calls `waitpid(-1, NULL, WNOHANG)` at the top of every main-loop iteration to silently reap any background child that has finished, preventing zombie accumulation.

### Signal handling

- The shell process ignores `SIGINT` and `SIGPIPE`.
- Forked foreground children restore `SIGINT` and `SIGPIPE` to `SIG_DFL` before executing. This means Ctrl-C kills foreground children but not the shell itself, and broken pipes in pipeline children terminate normally.
- Background children inherit `SIGINT = SIG_IGN` (the shell's disposition) and are reaped non-blocking at the top of each prompt loop.

### Built-in commands

Four built-ins must run in the shell process itself (not forkable):

- **`cd [DIR]`** — calls `chdir`. Updates `OLDPWD` before changing; `cd -` reads `OLDPWD` and prints the destination (matching bash behaviour). `cd` with no argument uses `$HOME`.
- **`exit [N]`** — exits with `N`, or with `last_status` if no argument.
- **`help`** — calls `reg_print_all()` to list all registered commands with their summaries, plus the built-in list.
- **`VAR=value`** — if the sole token on a line matches `[A-Za-z_][A-Za-z0-9_]*=...`, the shell calls `setenv(name, value, 1)`. Because `setenv` modifies the process environment, the new variable is immediately visible to all subsequent commands (including `$VAR` expansion in the tokenizer and child processes that inherit the environment).

### PATH integration

At startup, `setup_path` checks whether `~/.mysh/bin` exists. If so, it prepends it to `PATH` (avoiding double-prepending on nested shell invocations). This makes packages installed by `pkg install` immediately available as commands.

### Registry

The registry is a fixed-size flat array of `cmd_spec_t *` (max 64 entries). `reg_register` appends; `reg_find` is a linear scan by name. At 15 current commands this is negligible cost. Registration order determines the order entries appear in `help` output.

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

Reads all lines from all inputs into a single heap-allocated `char **` array, then calls `qsort_r` with a custom comparator. Comparator options (`reverse`, `numeric`, `key`, `sep`) are held in a `sort_ctx_t` struct on `sort_run`'s stack and passed through `qsort_r`'s context pointer, so concurrent invocations never share state. Field extraction for `-k` splits on whitespace (default) or on the `-t` separator character.

`-u` (unique): after sorting, a second pass marks adjacent equal lines `NULL` (freeing the string); the output pass skips NULLs. This avoids an auxiliary data structure.

### `uniq` — filter adjacent duplicates

Streams line-by-line, grouping adjacent equal lines into `entry_t` records `{text, len, hits}`. Does not require input to be sorted — only adjacent duplicates are collapsed, matching standard `uniq` behaviour. Pair with `sort | uniq` for global deduplication. Accepts optional positional INPUT and OUTPUT filenames.

### `cut` — field/character selection

Parses the LIST specification (`"1,3-5,7"`) into a `char selected[4096]` boolean array (1-based, index 0 unused) using `strtol` with range expansion. `-f` mode splits each line on the delimiter character (default TAB) and outputs selected fields re-joined by the same delimiter. `-c` mode selects byte positions directly. The two modes cannot be combined.

The 4096-element boolean array keeps the selection logic to a simple indexed lookup per field/character — no sorted-interval tree needed for the field counts typical in practice.

### `tee` — fanout stdin to multiple files

Opens all output files before reading. Reads stdin in 4 KiB chunks and `fwrite`s each chunk to stdout and every open file in the same loop iteration. This gives true simultaneous fanout with no extra buffering. JSON mode buffers the entire input first (to report `bytes_read`), then writes to files in the same pass.

### `pkg` — local package manager

Follows the full `cmd_spec_t` anatomy. Subcommands (`build`, `install`, `list`, `remove`) are dispatched manually by inspecting `argv[1]` — subcommand names are not argtable3 options — then each subcommand parses its own arguments with a dedicated `build_<sub>_argtable()` builder. Passing `argv + 1` to `arg_parse` makes the subcommand name the effective `argv[0]` for error messages, so argtable3 errors read naturally (e.g., `pkg build: missing option <src-dir>`). Each subcommand supports `--help` independently.

Subcommands: `build` (shells out to `tar -czf`), `install` (shells out to `tar -xzf` into a scratch directory, parses `pkg.json`, then moves the tree to `~/.mysh/pkgs/<name>-<ver>/` and symlinks declared binaries into `~/.mysh/bin/`; handles cross-filesystem moves via `cp` + `rm` fallback when `rename(2)` returns `EXDEV`), `list` (scans `~/.mysh/pkgs/` and reads each package's `pkg.json` for a description), `remove` (unlinks `~/.mysh/bin/` symlinks pointing into the package tree, then `rm -rf`; when no version is given, picks the lexicographically latest installed version).

Packages are described by a `pkg.json` manifest parsed with a minimal hand-written JSON reader (no library dependency beyond what the rest of the project already uses).

### `fetch` — TCP line client

The only networked command. It implements the canonical client sequence explicitly — `getaddrinfo` → `socket` → `connect` → `send` → `recv` — over a **line-based protocol**: the request is `MESSAGE` plus a trailing `\n`, and the reply is read until the terminating newline (detected with `memchr`) or until the peer closes. Arguments are `-H/--host` and `-p/--port` options plus a positional `MESSAGE`; `--json` selects machine-readable output.

Three robustness properties shape the implementation:

- **Input validation before any syscall.** The host must be non-empty, ≤ `FETCH_MAX_HOST_LEN` (255, the RFC 1035 limit), and free of control characters; the port must be in `1–65535`. These bounds are named constants at the top of the file.
- **Bounded receive.** After `connect`, the socket gets a 5-second `SO_RCVTIMEO` (`setsockopt` with a `struct timeval`), so a peer that accepts but never replies cannot hang the shell. A `recv` returning `EAGAIN`/`EWOULDBLOCK` is mapped to a `"timed out waiting for reply"` error.
- **No leaks on any path.** Every error path (`resolve`, `connect`, `setsockopt`, `send`, `recv`) closes the socket, frees the reply buffer, frees the argtable, and returns 1. `send_all` retries short writes and `EINTR`; the `socket`/`connect` loop walks every `addrinfo` candidate (so IPv4/IPv6 are both tried) and reports `strerror(errno)` only after all fail.

`fetch` honours the shell's I/O isolation: the reply (and the structured `--json` object) is written to the passed-in `out_stream`, while plain-text diagnostics go to `stderr` so stdout stays clean for pipelines. In `--json` mode the error object is written to `out_stream` instead, so an agent capturing stdout still receives a structured failure. The JSON schema is single-line with stable keys — success: `{ok:true, host, port, sent, bytes, reply}`; failure: `{ok:false, host, port, error}` — with `ok` as the discriminator. It is registered as a shell built-in, so it runs in-process inside `mysh`.

One acknowledged limitation: `connect()` itself is left blocking (no `SO_SNDTIMEO`, no non-blocking-connect + `select`), so an unreachable-but-routable host can stall at connect for the OS default before the recv timeout would ever apply. The 5-second bound covers the silent-peer case, which is the common failure in practice.

---

## Key design decisions

### argtable3 as the single source of truth

Every flag, its short name, its long name, its metavar, and its help string is declared exactly once — in `build_<name>_argtable()`. `print_usage` and `run` both call this builder. This means `--help` output is always consistent with what the parser actually accepts, and adding a new flag in one place automatically updates both.

### No shared helpers between apps

Each `cmd_*.c` defines its own `json_escape()` helper rather than sharing one from a common header. This is deliberate: it keeps each app a self-contained compilation unit with no dependency on sibling code, making it easy to lift an app out of the tree and use it elsewhere. The duplication (~15 lines per file) is a reasonable price for zero coupling.

### Dual-output binary + library

Every app builds to both `<name>` (standalone binary) and `lib<name>.a` (static library). The standalone binary is useful directly; the library is available if a future host program wants to embed a command. The shell uses neither — it links `cmd_<name>.o` directly to avoid duplicate `argtable3` symbols.

### In-process execution for registered commands

Registered commands run in-process for both single-stage and multi-stage invocations: no `fork`, no `exec`. For single-stage commands this means no process creation at all. For pipelines it means threads instead of forks — significantly cheaper, and it enables true concurrent data flow where stages overlap in time rather than running sequentially.

The tradeoff is that a misbehaving command (segfault, infinite loop) crashes or hangs the shell — acceptable in a course/educational context.

The critical enabler for in-process pipelines is the explicit `FILE *in_stream, FILE *out_stream` signature on every `run()` function. Because streams are passed as parameters, no shared fd state (fd 0/1) needs to be mutated, so multiple threads can run concurrently without any `dup2` or fd save/restore.

### Thread-safety of argtable3 builder

The stack-allocated `void *tbl[N]` pattern makes each builder invocation operate on its own stack frame. Multiple concurrent calls to the same builder are safe because no static or global data is shared.

`sort` uses `qsort_r` (GNU extension, available via `_GNU_SOURCE`) rather than `qsort`. `qsort_r` accepts a `void *` context pointer and forwards it to every comparator call, so all comparator options live in a stack-local `sort_ctx_t` struct. Two concurrent `sort` stages in the same pipeline are fully independent.

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

---

## Testing

### `test_apps.sh`

A Bash test runner at the repo root covers all apps and the shell's pipeline executor:

```sh
./test_apps.sh                    # run all 43 tests
./test_apps.sh --test N           # run a specific test by number
./test_apps.sh --test N --test M  # run multiple specific tests
```

The runner has two execution modes:

- **`run_test`** — invokes a binary directly, captures stdout+stderr, checks exit code, and optionally compares against an exact expected string.
- **`run_mysh`** — writes a pipeline string to a temp script file and runs it through `mysh`. Displays the pipeline string (not the script path) so the log is readable.

Exit code is 0 if all selected tests pass, 1 otherwise — suitable for use in a CI script.

### BNF shell lab tests (37–43)

Tests 37–43 cover the grammar-level features added in the BNF shell lab session:

| Test | What it exercises |
|---|---|
| 37 | `VAR=value` assignment, then `$VAR` expansion |
| 38 | `${VAR}` braced form inside double quotes |
| 39 | `$?` is 0 after a successful command |
| 40 | `$?` reflects a non-zero exit code (grep no match = 1) |
| 41 | Single-quoted `'$X'` is emitted literally, not expanded |
| 42 | `sleep 5 &` completes in < 3 s (non-blocking); output contains `[bg] PID` |
| 43 | `$VAR` expanded as an argument inside a pipeline (`grep $PATTERN`) |

### Thread-safety verification

Tests 33–35 directly exercise the `qsort_r` fix for `sort`. Each test runs a pipeline containing two concurrent `sort` threads and checks that the output is correct:

| Test | Pipeline | What it verifies |
|---|---|---|
| 33 | `cat \| sort \| uniq -c \| sort -rn` | Two sort threads with different flags don't race on comparator state |
| 34 | `cat \| grep \| sort \| uniq -c \| sort -rn` | Same, with grep adding a third concurrent thread |
| 35 | `cat \| grep \| sort \| uniq -c \| sort -rn \| head -n 3` | Pipeline terminates correctly when `head` closes its input pipe early |

Before the `qsort_r` fix, test 33 produced wrong output (all counts showed as 1) because the second `sort -rn` thread wrote `g_numeric=1` into the module-level global before the first `sort` thread called `qsort`, causing it to treat `#include`-style strings as numeric values (all comparing as 0, no reordering, so `uniq` saw no adjacent duplicates). The fix moved all comparator state into a `sort_ctx_t` struct passed through `qsort_r`'s context pointer, eliminating the shared mutable state entirely.
