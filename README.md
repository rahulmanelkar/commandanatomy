# rahulbox

A custom Unix shell ecosystem built in C. Each command follows a standard **Command Anatomy** (`cmd_spec_t`) that unifies CLI parsing, `--help` output, shell built-in registration, and package documentation — all driven by a single `argtable3` definition per command.

See [DESIGN.md](DESIGN.md) for full technical design documentation.

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
│   ├── sort/               # sort lines of text files
│   ├── uniq/               # report or omit repeated adjacent lines
│   ├── cut/                # remove sections from each line of files
│   ├── tee/                # read from stdin and write to stdout and files
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
make -C apps/sort
make -C apps/uniq
make -C apps/cut
make -C apps/tee
make -C apps/hello
make -C apps/pkg
```

Each app produces a standalone binary (e.g. `apps/ls/ls`) and a static library (e.g. `apps/ls/libls.a`). The shell links the `.o` files directly from each app so there is only one copy of `argtable3` in the final binary.

## Testing

`test_apps.sh` is a manual test runner that exercises every app and the shell's pipeline executor. Run it from the repo root after building:

```sh
./test_apps.sh                          # run all 43 tests
./test_apps.sh --test 1                 # run a single test by number
./test_apps.sh --test 1 --test 2        # run multiple specific tests
```

Each test prints the command being run, the actual output on pass, and a diff of expected vs. got on mismatch. Pipeline tests run through `mysh` and show the pipeline string directly:

```
Test 1    hello — default greeting
  $ ./apps/hello/hello
  PASS
    Hello, World!

Test 33   pipe 4-stage: cat | sort | uniq -c | sort -rn  (2 concurrent sort threads)
  [mysh]$ cat /tmp/rb_fruits.txt | sort | uniq -c | sort -rn
  PASS
          2 apple
          2 banana
          1 cherry
```

A summary line is printed at the end and the script exits non-zero if any test failed.

**Test index:**

| Range | App / feature |
|---|---|
| 1–2 | `hello` — greeting, custom name |
| 3–4 | `ls` — directory listing, JSON output |
| 5 | `stat` — file metadata |
| 6–7 | `wc` — default counts, line-only |
| 8–10 | `cat` — plain, numbered lines, show-ends |
| 11–13 | `echo` — basic, escape sequences, suppress newline |
| 14–15 | `head` / `tail` — first N and last N lines |
| 16–18 | `grep` — match, count, invert |
| 19–24 | `sort` — alpha, reverse, numeric, numeric-reverse, unique, field sort |
| 25–26 | `uniq` — adjacent dedup, count occurrences |
| 27–28 | `cut` — field extraction, character range |
| 29 | `tee` — stdout passthrough + file write verified |
| 30–36 | Pipelines — 2- through 6-stage concurrent thread pipelines through `mysh` |
| 37–43 | BNF shell lab — `VAR=value` assignment, `$VAR`/`${VAR}`/`$?` expansion, single-quote suppression, `&` background, `$VAR` as pipeline argument |

Test 33 specifically targets the thread-safety fix for `sort`: it runs two `sort` stages concurrently in the same pipeline (`cat | sort | uniq -c | sort -rn`) and verifies they produce correct output despite sharing the same process address space.

---

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

### `sort` — sort lines of text files

```sh
apps/sort/sort [OPTIONS] [FILE...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-n`, `--numeric-sort` | Compare fields as numeric values |
| `-r`, `--reverse` | Reverse the sort order |
| `-u`, `--unique` | Output only the first of equal lines |
| `-k FIELD`, `--key=FIELD` | Sort by column number (1-based) |
| `-t SEP` | Field separator (default: whitespace) |
| `--json` | Machine-readable JSON output |

Reads from stdin when no files are given. All inputs are merged before sorting.

**Examples:**

```sh
# Alphabetic sort
apps/sort/sort apps/ls/cmd_ls.c

# Numeric sort, reverse
printf "3\n1\n10\n2\n" | apps/sort/sort -n -r
# 10 3 2 1

# Sort and deduplicate
printf "banana\napple\nbanana\n" | apps/sort/sort -u
# apple banana

# Sort CSV by second column numerically
printf "alice,30\nbob,25\ncarol,35\n" | apps/sort/sort -t, -k2 -n
# bob,25 alice,30 carol,35

# JSON output
printf "cherry\napple\nbanana\n" | apps/sort/sort --json
# ["apple", "banana", "cherry"]
```

---

### `uniq` — report or omit repeated adjacent lines

```sh
apps/uniq/uniq [OPTIONS] [INPUT [OUTPUT]]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-c`, `--count` | Prefix lines with number of occurrences |
| `-d`, `--repeated` | Only print lines that occur more than once |
| `-u`, `--unique` | Only print lines that occur exactly once |
| `--json` | Machine-readable JSON output |

Operates on **adjacent** lines — pipe through `sort` first to deduplicate globally.

**Examples:**

```sh
# Remove adjacent duplicates
printf "a\na\nb\nc\nc\n" | apps/uniq/uniq
# a b c

# Show counts
printf "a\na\nb\nc\nc\nc\n" | apps/uniq/uniq -c
#       2 a
#       1 b
#       3 c

# Show only lines that appear more than once
printf "a\na\nb\nc\nc\n" | apps/uniq/uniq -d
# a c

# Full dedup pipeline: sort then uniq
cat apps/*/cmd_*.c | grep "^#include" | sort | apps/uniq/uniq -c | sort -rn | head -5

# JSON output
printf "a\na\nb\n" | apps/uniq/uniq --json
# [{"count": 2, "line": "a"}, {"count": 1, "line": "b"}]
```

---

### `cut` — remove sections from each line of files

```sh
apps/cut/cut [OPTIONS] [FILE...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-f LIST`, `--fields=LIST` | Select fields (e.g. `1,3` or `1-3,5`) |
| `-c LIST`, `--characters=LIST` | Select character positions |
| `-d DELIM`, `--delimiter=DELIM` | Field delimiter (default: TAB) |
| `--json` | Machine-readable JSON output |

`LIST` is a comma-separated sequence of numbers or ranges (e.g. `1,3-5,7`). Use `-f` for delimited fields or `-c` for byte positions; they cannot be combined.

**Examples:**

```sh
# First field of colon-delimited file
cut -f1 -d: /etc/passwd | apps/cut/cut -f1 -d:

# Fields 1 and 3 from CSV
printf "a,b,c\n1,2,3\n" | apps/cut/cut -f1,3 -d,
# a,c
# 1,3

# Character positions 1-3
printf "abcdef\n123456\n" | apps/cut/cut -c1-3
# abc
# 123

# Extract filename extensions
ls apps/cat/ | apps/cut/cut -f2 -d.

# JSON output
printf "foo:bar:baz\n" | apps/cut/cut -d: -f2 --json
# [{"lines": ["bar"]}]
```

---

### `tee` — read from stdin and write to stdout and files

```sh
apps/tee/tee [OPTIONS] [FILE...]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit |
| `-a`, `--append` | Append to files instead of overwriting |
| `--json` | Machine-readable JSON output |

Reads all of stdin and writes it simultaneously to standard output and each `FILE`. Useful mid-pipeline when you want to capture an intermediate result without breaking the pipe.

**Examples:**

```sh
# Capture intermediate output while passing through
ls apps/ | apps/tee/tee /tmp/app_list.txt | wc -l

# Append to a log
echo "build started" | apps/tee/tee -a build.log

# Write the same content to multiple files
echo "hello" | apps/tee/tee file1.txt file2.txt

# JSON output (captures content + byte count)
echo "hello world" | apps/tee/tee --json
# {"bytes_read": 12, "content": "hello world\n"}
```

---

### `pkg` — local package manager

```sh
apps/pkg/pkg <subcommand> [--help]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help and exit (top-level or per-subcommand) |

Follows the full `cmd_spec_t` anatomy with `argtable3`. Each subcommand has its own argument definition and `--help` output. Run `apps/pkg/pkg --help` for the subcommand list; run `apps/pkg/pkg <subcommand> --help` for per-subcommand usage.

Packages are `.tar.gz` archives containing a `pkg.json` manifest plus any files to install. They install under `~/.mysh/pkgs/<name>-<version>/` and declared binaries are symlinked into `~/.mysh/bin/`. `pkg` is also registered as a shell built-in, so it runs in-process inside `mysh` with no fork overhead.

**`pkg.json` format:**

```json
{
  "name": "mypkg",
  "version": "1.0.0",
  "description": "What this package does",
  "bin": ["myscript.sh"]
}
```

| Subcommand | Usage | Description |
|---|---|---|
| `build` | `build <src-dir> <output.tar.gz>` | Pack a directory into a package archive |
| `install` | `install <pkg.tar.gz>` | Extract and install; symlink declared binaries |
| `list` | `list` | List installed packages with descriptions |
| `remove` | `remove <name> [version]` | Remove a package and its bin symlinks |

**Examples:**

```sh
# Show all subcommands
apps/pkg/pkg --help

# Per-subcommand help
apps/pkg/pkg build --help

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

`mysh` is a small Unix shell that runs all registered commands in-process and falls back to `fork` + `execvp` for anything else in `PATH`.

- **Single-stage commands** run directly in the shell process — no fork, no exec.
- **Multi-stage pipelines** run each registered stage as a concurrent POSIX thread, connected by `pipe()` + `fdopen()`. External commands in the same pipeline still fork. Stages overlap in time: downstream threads start consuming data before upstream threads finish producing it.

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
| Registered commands (in-process, no fork) | `ls -a`, `wc -l file`, `cat file`, `echo hello`, `head -n 5 file`, `tail -n 5 file`, `grep foo file`, `sort -n`, `uniq -c`, `cut -f1 -d:`, `tee out.txt`, `stat file` |
| Concurrent threaded pipelines | `cat file \| grep pat \| sort \| uniq -c` — stages run as threads, not forks |
| External commands (via PATH) | `git status`, `python3 script.py` |
| Input redirection | `wc -l < file.txt` |
| Output redirection | `ls > out.txt` |
| Append redirection | `echo "line" >> log.txt` |
| Pipelines | `ls \| sort \| wc -l` |
| Variable assignment | `NAME=Alice` — sets an environment variable in the shell |
| Variable expansion | `echo $NAME`, `echo ${NAME}`, `echo $?` (last exit status) |
| Single and double quoting | `echo 'literal $X'` (no expansion), `echo "hello $NAME"` (expands) |
| Background execution | `sleep 10 &` — runs in background, prints `[bg] PID`, shell continues |
| Comments | `# this line is ignored` |
| Script files | `mysh deploy.sh` |

### Shell syntax reference

#### Variable assignment

```sh
NAME=Alice
COUNT=42
_TEMP=hello
```

A bare word matching `VAR=value` (no spaces around `=`, a single token on the line) sets an environment variable in the shell process. The name must start with a letter or `_` and contain only letters, digits, and `_`. The assigned value is visible to all subsequent commands and child processes.

```sh
mysh> GREETING=hello
mysh> echo $GREETING
hello
mysh> git commit -m "$GREETING from mysh"   # inherited by forked children
```

A token like `FOO=bar baz` is **not** an assignment — the space splits it into two tokens and the line runs as a command instead.

#### Variable expansion

| Syntax | Expands to |
|--------|------------|
| `$VAR` | Value of environment variable `VAR` |
| `${VAR}` | Same; braces required when the name is adjacent to other text |
| `$?` | Exit status of the last foreground command |

Expansion happens during tokenisation, before the command runs. Undefined variables expand to an empty string.

```sh
mysh> HOST=db-server
mysh> echo "connecting to ${HOST}:5432"
connecting to db-server:5432

mysh> grep pattern file.txt
mysh> echo $?
0                        # or 1 if no match

mysh> PATTERN=include
mysh> grep $PATTERN apps/cat/cmd_cat.c | wc -l
5
```

`${VAR}` is needed when the name must be separated from surrounding text:

```sh
mysh> BASE=hello
mysh> echo ${BASE}_world   # → hello_world
mysh> echo $BASE_world     # → empty: looks up "BASE_world"
```

#### Quoting

**Single quotes** — everything between `'...'` is completely literal. No `$` expansion, no backslash escapes, no special characters.

```sh
mysh> echo 'Price: $5.00 (no expansion)'
Price: $5.00 (no expansion)

mysh> echo 'tab:\t  newline:\n'   # no escape processing
tab:\t  newline:\n
```

**Double quotes** — `$VAR`, `${VAR}`, and `$?` expand as normal. The only recognised backslash escapes are `\"`, `\\`, and `\$`; any other `\` is kept literally.

```sh
mysh> NAME=world
mysh> echo "Hello, $NAME!"
Hello, world!

mysh> echo "A quote: \" and a dollar: \$NAME"
A quote: " and a dollar: $NAME

mysh> echo "Path: $HOME/bin"
Path: /home/user/bin
```

**Empty quotes** — `''` and `""` produce an empty-string argument (unlike bare whitespace which produces no token):

```sh
mysh> wc -l ''       # passes an empty filename — wc will error
mysh> echo ""        # prints a blank line
```

#### Pipelines

Stages are separated by `|`. All registered commands run as concurrent POSIX threads; external commands fork. Data streams through `pipe()` connections — downstream stages start consuming before upstream stages finish.

```sh
mysh> cat apps/*/cmd_*.c | grep "^#include" | sort | uniq -c | sort -rn | head -5
```

Up to 16 stages per pipeline.

#### I/O redirection

| Syntax | Effect |
|--------|--------|
| `cmd < file` | Read stdin from `file` |
| `cmd > file` | Write stdout to `file` (truncate) |
| `cmd >> file` | Write stdout to `file` (append) |

Redirection can be combined with pipelines — `<` applies to the first stage, `>` / `>>` to the last stage.

```sh
mysh> wc -l < apps/cat/cmd_cat.c
mysh> grep include apps/cat/cmd_cat.c > /tmp/includes.txt
mysh> echo "run at $(date)" >> run.log   # date is an external command here
mysh> cat < input.txt | sort | uniq > output.txt
```

#### Background execution

Append `&` to run the entire command (or pipeline) in the background. The shell prints `[bg] PID` and returns to the prompt immediately. Background jobs are reaped automatically (no explicit `wait` needed).

```sh
mysh> sleep 30 &
[bg] 12345
mysh> echo "shell is free"

mysh> cat huge_file.txt | sort > sorted.txt &
[bg] 12346
```

`$?` after `cmd &` is always 0 — the job's exit status is not tracked.

#### Comments

A `#` that appears at the start of a word (not inside quotes, not adjacent to another word) begins a comment. The rest of the line is ignored.

```sh
mysh> # this entire line is a comment
mysh> ls apps   # trailing comment — also ignored
mysh> echo '#not a comment'   # inside single quotes: literal
#not a comment
```

#### Script files

Pass a filename as the first argument to run it as a script. All shell syntax works identically in scripts.

```sh
# deploy.sh
DEST=/tmp/release
mkdir -p $DEST
cat README.md | grep "^#" > $DEST/headers.txt
echo "done" >> $DEST/build.log
```

```sh
shell/mysh deploy.sh
```

Scripts also accept stdin:

```sh
echo 'ls apps | wc -l' | shell/mysh
```

---

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

### Thread-safe pipeline execution

When all stages in a pipeline are registered commands, every stage runs as a concurrent POSIX thread. Pipes connect them: each thread reads from `FILE *in` and writes to `FILE *out`, which are backed by `fdopen`'d pipe ends. No thread ever calls `dup2` — the streams are passed explicitly, so threads do not race on fd 0/1.

The architecture has two properties that make this safe:

**Explicit stream parameters.** Every `run()` function is declared:
```c
int (*run)(int argc, char **argv, FILE *in_stream, FILE *out_stream);
```
Threads receive isolated `FILE*` objects constructed from pipe fds. Each thread closes its streams on exit, which signals EOF to the downstream thread.

**Stack-allocated argtable3 tables.** Each `build_<name>_argtable()` call writes into a `void *tbl[N]` the caller places on its own stack frame. Two concurrent calls to the same builder cannot share state.

**`sort` uses `qsort_r`.** Comparator options (`-r`, `-n`, `-k`, `-t`) are held in a `sort_ctx_t` struct on the stack and passed through `qsort_r`'s context pointer. Two concurrent `sort` stages in the same pipeline are fully independent.

**Try it — all-internal 5-stage pipeline:**
```sh
mysh> cat apps/hello/cmd_hello.c | grep include | sort | uniq -c | sort -rn
```
All five stages (`cat`, `grep`, `sort`, `uniq`, `sort`) run as concurrent threads. Data flows through four pipes simultaneously.

**Watch with `strace` to confirm no forks for internal commands:**
```sh
strace -e trace=clone,fork,vfork -f shell/mysh 2>&1 <<'EOF'
cat apps/cat/cmd_cat.c | grep include | sort -r | uniq
EOF
```
You will see `clone` calls (threads) for the registered stages and no `fork`/`clone` with `SIGCHLD` (process fork) unless a stage falls back to an external binary.

**`tee` mid-pipeline — one thread fans out to stdout and a file simultaneously:**
```sh
mysh> cat apps/cat/cmd_cat.c | grep include | tee /tmp/includes.txt | wc -l
```
The `tee` thread writes to both the pipe (going to `wc`) and the file in the same read loop iteration — no extra buffering, no second pass.

**Long dedup pipeline — canonical use case:**
```sh
mysh> cat apps/*/cmd_*.c | grep "^#include" | sort | uniq -c | sort -rn | head -5
```
Six stages run as six threads. `cat` starts writing; `grep` starts filtering before `cat` finishes; `sort` buffers but begins as soon as `grep` closes its pipe; `uniq` and the second `sort` follow in turn; `head` exits after 5 lines, closing its pipe and causing `sort` to receive SIGPIPE upstream.

**Isolation from the shell's fd 0.** When `mysh` reads commands from a pipe or script file, it `dup`s that fd to a private `g_cmd_fd` and leaves fd 0 alone. Pipeline threads receive `fdopen(dup(STDIN_FILENO), "r")` for the first stage, so they can safely `fclose` their input without touching the shell's command stream.

### Example session

```sh
mysh> ls apps
cat  cut  echo  grep  head  hello  ls  pkg  sort  stat  tail  tee  uniq  wc

mysh> echo "one two three" | wc -w
       3

mysh> echo -e "line1\nline2\nline3" | head -n 2
line1
line2

mysh> echo -e "line1\nline2\nline3" | tail -n 1
line3

mysh> grep -n include apps/cat/cmd_cat.c | head -n 5
1:#include <stdio.h>
2:#include <stdlib.h>
3:#include <string.h>
4:#include <errno.h>
5:#include "argtable3.h"

mysh> grep "^#include" apps/*/cmd_*.c | cut -f2 -d: | sort | uniq -c | sort -rn | head -5
      9 #include <stdio.h>
      9 #include <stdlib.h>
      9 #include <string.h>
      8 #include "argtable3.h"
      8 #include "cmd_spec.h"

mysh> ls apps | tee /tmp/apps.txt | wc -l
      14

mysh> cat apps/hello/hello_main.c | wc -l
       8

mysh> wc -l < apps/pkg/cmd_pkg.c
     708 apps/pkg/cmd_pkg.c

mysh> NAME=world
mysh> echo "Hello $NAME"
Hello world

mysh> PATTERN=include
mysh> grep $PATTERN apps/cat/cmd_cat.c | wc -l
5

mysh> echo $?
0

mysh> sleep 10 &
[bg] 12345
mysh> echo "shell continues immediately"
shell continues immediately

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
  echo             display a line of text
  head             print the first lines of files
  tail             print the last lines of files
  grep             search for patterns in files
  sort             sort lines of text files
  uniq             report or omit repeated adjacent lines
  cut              remove sections from each line of files
  tee              read from stdin and write to stdout and files

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
    int  (*run)(int argc, char **argv, FILE *in_stream, FILE *out_stream);
    void (*print_usage)(FILE *out);
} cmd_spec_t;
```

The `FILE *in_stream, FILE *out_stream` parameters are the key to thread-safe pipeline execution: the shell passes pipe-backed `FILE*` objects to each thread instead of redirecting fd 0/1, so concurrent stages never race on shared file descriptors.

Each module uses a shared `build_<name>_argtable()` helper — with the table allocated on the caller's stack — so that `run()` and `print_usage()` always stay in sync and concurrent invocations never share mutable state. This makes `argtable3` the single source of truth for CLI parsing, `--help` text, and future package documentation.

The `apps/hello/` module is the canonical reference implementation. It also includes `registry.c` — a minimal in-memory command registry (`registry_register`, `registry_find`, `registry_print_all`) that the shell will use for built-in dispatch.

See `CommandAnatomy.ipynb` for a full walkthrough of the design.
