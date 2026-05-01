# rahulbox

A custom Unix shell ecosystem built in C. Each command follows a standard **Command Anatomy** (`cmd_spec_t`) that unifies CLI parsing, `--help` output, shell built-in registration, and package documentation — all driven by a single `argtable3` definition per command.

## Project structure

```
rahulbox/
├── include/
│   └── cmd_spec.h          # shared cmd_spec_t definition
├── vendor/
│   └── argtable3/          # vendored argument parsing library
├── apps/
│   ├── hello/              # reference implementation of the anatomy
│   ├── ls/                 # list directory contents
│   ├── stat/               # display file status
│   ├── wc/                 # count lines, words, and bytes
│   └── cat/                # concatenate and print files
├── Makefile                # top-level build coordinator
└── CommandAnatomy.ipynb    # course notebook and architecture guide
```

## Requirements

- Linux
- GCC
- GNU Make

No other dependencies — `argtable3` is vendored under `vendor/`.

## Building

Build all commands from the project root:

```sh
make
```

Clean all build artifacts:

```sh
make clean
```

Build a single command:

```sh
make -C apps/ls
make -C apps/stat
make -C apps/wc
make -C apps/cat
make -C apps/hello
```

Binaries are produced inside each app's directory (e.g. `apps/ls/ls`). Each app also produces a static library (e.g. `apps/ls/libls.a`) for linking into the shell.

## Commands

### `hello` — reference implementation

```sh
apps/hello/hello [OPTIONS]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-n`, `--name=NAME` | Whom to greet (default: World) |

The canonical example of the `cmd_spec_t` anatomy. Copy this module when implementing a new command.

**Examples:**

```sh
apps/hello/hello
# Hello, World!

apps/hello/hello --name Alice
# Hello, Alice!
```

---

### `ls` — list directory contents

```sh
apps/ls/ls [OPTIONS] [PATH...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-a`, `--all` | Include entries starting with `.` |
| `--json` | Machine-readable JSON output |

**Examples:**

```sh
# List current directory
apps/ls/ls

# Show hidden files
apps/ls/ls -a /tmp

# JSON output (single directory)
apps/ls/ls --json apps/ls/
# {"path": "apps/ls/", "entries": [{"name": "cmd_ls.c", "type": "file", "size": 7085}, ...]}

# JSON output (multiple paths) — returns a JSON array
apps/ls/ls --json apps/ls/ apps/stat/
# [{"path": "apps/ls/", ...}, {"path": "apps/stat/", ...}]
```

---

### `stat` — display file status

```sh
apps/stat/stat [OPTIONS] FILE
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `--json` | Machine-readable JSON output |

Displays: size, blocks, inode, device, hard link count, permissions (symbolic + octal), uid/gid with names, and access/modify/change timestamps. Uses `lstat`, so symlinks are reported as symlinks rather than their targets.

**Examples:**

```sh
# Human-readable metadata
apps/stat/stat apps/ls/cmd_ls.c

# JSON output
apps/stat/stat --json apps/ls/cmd_ls.c
# {"path": "apps/ls/cmd_ls.c", "size": 7085, "mode_str": "-rw-rw-r--", ...}
```

---

### `wc` — count lines, words, and bytes

```sh
apps/wc/wc [OPTIONS] [FILE...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-l`, `--lines` | Print newline count |
| `-w`, `--words` | Print word count |
| `-c`, `--bytes` | Print byte count |
| `-m`, `--chars` | Print character count |
| `--json` | Machine-readable JSON output |

With no mode flag, defaults to `-lwc` (lines, words, bytes). Reads stdin when no files are given. When multiple files are provided, prints a `total` row.

**Examples:**

```sh
# Default: lines, words, bytes
apps/wc/wc apps/wc/cmd_wc.c
#      237     810    7429 apps/wc/cmd_wc.c

# Lines only
apps/wc/wc -l apps/wc/cmd_wc.c

# Multiple files with totals
apps/wc/wc apps/wc/cmd_wc.c apps/cat/cmd_cat.c

# Count words from stdin
echo "hello world" | apps/wc/wc -w

# JSON output
apps/wc/wc --json apps/wc/cmd_wc.c
# [{"file": "apps/wc/cmd_wc.c", "lines": 237, "words": 810, "bytes": 7429}]

# JSON with multiple files (includes total entry)
apps/wc/wc --json apps/wc/cmd_wc.c apps/cat/cmd_cat.c
# [{"file": "...", ...}, {"file": "...", ...}, {"file": "total", ...}]
```

---

### `cat` — concatenate files and print to standard output

```sh
apps/cat/cat [OPTIONS] [FILE...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-n`, `--number` | Number all output lines |
| `-E`, `--show-ends` | Display `$` at end of each line |
| `--json` | Machine-readable JSON output |

Reads stdin when no files are given or when `FILE` is `-`. Options `-n` and `-E` can be combined.

**Examples:**

```sh
# Print a file
apps/cat/cat apps/cat/cmd_cat.c

# Number lines
apps/cat/cat -n apps/cat/cmd_cat.c

# Show line endings
apps/cat/cat -E apps/cat/cmd_cat.c

# Concatenate two files
apps/cat/cat apps/cat/cat_main.c apps/wc/wc_main.c

# Read from stdin
echo "hello" | apps/cat/cat

# JSON output (content as escaped string)
apps/cat/cat --json apps/cat/cat_main.c
# [{"file": "apps/cat/cat_main.c", "content": "#include <stdlib.h>\n..."}]

# JSON from stdin
echo "hello" | apps/cat/cat --json
# [{"content": "hello\n"}]
```

## Command Anatomy

Every command in this project follows a standard module structure defined in `include/cmd_spec.h`:

```c
typedef struct cmd_spec {
    const char *name;
    const char *summary;
    const char *long_help;
    int  (*run)(int argc, char **argv);
    void (*print_usage)(FILE *out);
} cmd_spec_t;
```

Each module uses a shared `build_<name>_argtable()` helper so that `run()` and `print_usage()` always stay in sync. This makes `argtable3` the single source of truth for CLI parsing, `--help` text, and future package documentation.

The `apps/hello/` module is the canonical reference implementation. It also includes `registry.c` — a minimal in-memory command registry (`registry_register`, `registry_find`, `registry_print_all`) that the shell will use for built-in dispatch.

See `CommandAnatomy.ipynb` for a full walkthrough of the design.
