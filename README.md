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
│   ├── cat/                # concatenate and print files
│   ├── echo/               # display a line of text
│   ├── head/               # print the first lines of files
│   ├── tail/               # print the last lines of files
│   ├── grep/               # search for patterns in files
│   └── pkg/                # local package manager
├── shell/
│   ├── mysh.c              # shell main loop + execution engine
│   ├── tok.c / tok.h       # quote-aware tokenizer
│   └── Makefile
├── Makefile                # top-level build coordinator
└── CommandAnatomy.ipynb    # course notebook and architecture guide
```

## Requirements

- Linux
- GCC
- GNU Make

No other dependencies — `argtable3` is vendored under `vendor/`.

## Building

Build everything (all commands + the shell) from the project root:

```sh
make
```

Clean all build artifacts:

```sh
make clean
```

Build only the shell (also builds any app dependencies):

```sh
make shell
# or directly:
make -C shell
```

Build a single command:

```sh
make -C apps/ls
make -C apps/wc
make -C apps/cat
make -C apps/echo
make -C apps/head
make -C apps/tail
make -C apps/grep
make -C apps/hello
make -C apps/pkg
```

Each app produces a standalone binary (e.g. `apps/ls/ls`) and a static library (e.g. `apps/ls/libls.a`). The shell links the `.o` files directly from each app so there is only one copy of `argtable3` in the final binary.

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

---

### `echo` — display a line of text

```sh
apps/echo/echo [OPTIONS] [STRING...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-n` | Do not output trailing newline |
| `-e` | Enable interpretation of backslash escapes (`\n`, `\t`, `\\`, etc.) |
| `--json` | Machine-readable JSON output |

**Examples:**

```sh
apps/echo/echo hello world
# hello world

apps/echo/echo -n "no newline"

apps/echo/echo -e "line1\nline2"

apps/echo/echo --json hello world
# {"output": "hello world\n"}
```

---

### `head` — print the first lines of files

```sh
apps/head/head [OPTIONS] [FILE...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-n NUM`, `--lines=NUM` | Print first NUM lines (default: 10) |
| `-c NUM`, `--bytes=NUM` | Print first NUM bytes |
| `--json` | Machine-readable JSON output |

Reads stdin when no files are given. With multiple files, prints a `==> file <==` header before each. Use `-` as a filename to read from stdin.

**Examples:**

```sh
# First 10 lines of a file
apps/head/head apps/cat/cmd_cat.c

# First 5 lines
apps/head/head -n 5 apps/cat/cmd_cat.c

# First 100 bytes
apps/head/head -c 100 apps/cat/cmd_cat.c

# JSON output
apps/head/head --json -n 3 apps/cat/cmd_cat.c
# [{"file": "apps/cat/cmd_cat.c", "content": "#include <stdio.h>\n..."}]
```

---

### `tail` — print the last lines of files

```sh
apps/tail/tail [OPTIONS] [FILE...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-n NUM`, `--lines=NUM` | Print last NUM lines (default: 10) |
| `-c NUM`, `--bytes=NUM` | Print last NUM bytes |
| `--json` | Machine-readable JSON output |

Reads stdin when no files are given. With multiple files, prints a `==> file <==` header before each.

**Examples:**

```sh
# Last 10 lines
apps/tail/tail apps/cat/cmd_cat.c

# Last 5 lines
apps/tail/tail -n 5 apps/cat/cmd_cat.c

# Last 50 bytes
apps/tail/tail -c 50 apps/cat/cmd_cat.c

# JSON output
apps/tail/tail --json -n 3 apps/cat/cmd_cat.c
# [{"file": "apps/cat/cmd_cat.c", "content": "...\n"}]
```

---

### `grep` — search for patterns in files

```sh
apps/grep/grep [OPTIONS] PATTERN [FILE...]
```

`PATTERN` is a POSIX extended regular expression.

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-i`, `--ignore-case` | Ignore case distinctions in PATTERN |
| `-n`, `--line-number` | Prefix each match with its line number |
| `-c`, `--count` | Print only a count of matching lines per file |
| `-v`, `--invert-match` | Select non-matching lines |
| `-l`, `--files-with-matches` | Print only names of files containing matches |
| `--json` | Machine-readable JSON output |

Exit status: 0 if a match was found, 1 if no match, 2 on error.

**Examples:**

```sh
# Basic search
apps/grep/grep include apps/cat/cmd_cat.c

# Case-insensitive with line numbers
apps/grep/grep -in stdio apps/cat/cmd_cat.c

# Count matches per file
apps/grep/grep -c include apps/cat/cmd_cat.c apps/wc/cmd_wc.c

# Invert match (non-matching lines)
apps/grep/grep -v '^#' apps/cat/cmd_cat.c

# Files containing the pattern
apps/grep/grep -l arg_lit apps/*/cmd_*.c

# JSON output
apps/grep/grep --json include apps/cat/cmd_cat.c
# [{"file": "apps/cat/cmd_cat.c", "matches": [{"line": 1, "text": "#include <stdio.h>\n"}, ...]}]
```

---

### `pkg` — local package manager

```sh
apps/pkg/pkg <subcommand> [args]
```

Packages are `.tar.gz` archives containing a `pkg.json` manifest plus any files to install. They install under `~/.mysh/pkgs/<name>-<version>/` and declared binaries are symlinked into `~/.mysh/bin/`.

**`pkg.json` format:**

```json
{
  "name": "mypkg",
  "version": "1.0.0",
  "description": "What this package does",
  "bin": ["myscript.sh"]
}
```

| Subcommand | Description |
|---|---|
| `build <src-dir> <output.tar.gz>` | Pack a directory into a package archive |
| `install <pkg.tar.gz>` | Extract and install; symlink declared binaries |
| `list` | List installed packages with descriptions |
| `remove <name> [version]` | Remove a package and its bin symlinks |

**Examples:**

```sh
# Pack a directory
apps/pkg/pkg build myapp/ myapp-1.0.0.tar.gz

# Install it
apps/pkg/pkg install myapp-1.0.0.tar.gz
# Installed to ~/.mysh/pkgs/myapp-1.0.0/
# Symlinked ~/.mysh/bin/myscript.sh -> ~/.mysh/pkgs/myapp-1.0.0/myscript.sh

# List what's installed
apps/pkg/pkg list

# Remove by name (latest version) or exact version
apps/pkg/pkg remove myapp
apps/pkg/pkg remove myapp 1.0.0
```

`~/.mysh/bin` is automatically added to `PATH` by the shell on startup, so installed binaries are immediately available after `pkg install`.

---

## Shell (`mysh`)

```sh
shell/mysh              # interactive mode
shell/mysh script.sh    # run a script file
```

`mysh` is a small Unix shell that runs all registered commands in-process (no fork overhead for `ls`, `wc`, `cat`, etc.) and falls back to `fork` + `execvp` for anything else in `PATH`.

### Starting the shell

```sh
# Build and launch interactively
make && shell/mysh
```

```
mysh — type 'help' for available commands, 'exit' to quit
mysh>
```

The prompt shows the last exit code when non-zero: `mysh [1]> `.

### Features

| Feature | Example |
|---|---|
| Registered commands (in-process) | `ls -a`, `wc -l file`, `cat file`, `echo hello`, `head -n 5 file`, `tail -n 5 file`, `grep foo file`, `hello --name X`, `stat file` |
| External commands (via PATH) | `git status`, `python3 script.py` |
| Input redirection | `wc -l < file.txt` |
| Output redirection | `ls > out.txt` |
| Append redirection | `echo "line" >> log.txt` |
| Pipelines | `ls \| sort \| wc -l` |
| Single and double quoting | `echo 'hello world'`, `echo "hi $USER"` |
| Comments | `# this line is ignored` |
| Script files | `mysh deploy.sh` |

### Built-in commands

| Command | Description |
|---|---|
| `cd [DIR]` | Change directory; `cd` alone goes to `$HOME`; `cd -` returns to previous directory |
| `exit [N]` | Exit with optional status code (defaults to last exit status) |
| `help` | List all built-ins and registered commands |

### PATH integration

On startup, `mysh` prepends `~/.mysh/bin` to `PATH` if it exists. Combined with `pkg install`, this means installed packages are available immediately:

```sh
apps/pkg/pkg install mytool-1.0.0.tar.gz
shell/mysh
mysh> mytool          # found via ~/.mysh/bin
```

### Example session

```sh
mysh> ls apps
hello  ls  stat  wc  cat  pkg

mysh> echo "one two three" | wc -w
       3

mysh> cat apps/hello/hello_main.c | wc -l
       8

mysh> wc -l < apps/pkg/pkg.c
     591 apps/pkg/pkg.c

mysh> cd /tmp && pwd
/tmp

mysh> cd -
/home/user/rahulbox

mysh> help
Built-in commands:
  cd [DIR]         change working directory (default: $HOME)
  exit [N]         exit the shell with optional status N
  help             show this help

Registered commands:
  hello            print a friendly greeting
  ls               list directory contents
  stat             display file status
  wc               count lines, words, and bytes
  cat              concatenate files and print to standard output

All other commands are looked up in PATH.

mysh> exit
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
