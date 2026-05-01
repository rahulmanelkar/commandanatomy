# CLItools — Required Shell Commands and Flags

This file is the authoritative spec for every command in the rahulbox shell ecosystem. When implementing or refactoring a command using the `cmd_spec_t` anatomy and `argtable3`, use this document to ensure the correct names and flags are used.

---

## Universal conventions

Every command must support:

| Flag | Description |
|---|---|
| `-h`, `--help` | Print usage and exit with status 0 |
| `--json` | Emit machine-readable JSON to stdout (for agent/MCP integration) |

`--json` output must be valid JSON and written to stdout. Error messages always go to stderr regardless of `--json`.

---

## Commands

### `hello`

Reference implementation of the `cmd_spec_t` anatomy.

```
hello [-h] [-n NAME]
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show help and exit |
| `-n NAME`, `--name=NAME` | Whom to greet (default: `World`) |

**JSON output:** not required (hello is a teaching tool, not a data command).

---

### `ls`

List directory contents.

```
ls [-h] [-a] [--json] [PATH...]
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show help and exit |
| `-a`, `--all` | Do not ignore entries starting with `.` |
| `--json` | Emit entries as JSON |

**JSON output (single path):**
```json
{"path": "dir/", "entries": [{"name": "foo", "type": "file", "size": 123}]}
```

**JSON output (multiple paths):** a JSON array of the above objects.

`type` is one of: `file`, `directory`, `symlink`, `fifo`, `socket`, `block`, `char`.

---

### `stat`

Display file or directory metadata.

```
stat [-h] [--json] FILE
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show help and exit |
| `--json` | Emit metadata as JSON |

Uses `lstat` so symlinks are reported as symlinks, not their targets.

**JSON output fields:** `path`, `size`, `blocks`, `inode`, `device`, `nlink`, `mode_str` (e.g. `-rw-rw-r--`), `mode_oct`, `uid`, `gid`, `owner`, `group`, `atime`, `mtime`, `ctime` (all times as ISO 8601 strings).

---

### `wc`

Count lines, words, and bytes in files.

```
wc [-h] [-l] [-w] [-c] [-m] [--json] [FILE...]
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show help and exit |
| `-l`, `--lines` | Print newline count |
| `-w`, `--words` | Print word count |
| `-c`, `--bytes` | Print byte count |
| `-m`, `--chars` | Print character count |
| `--json` | Emit counts as JSON |

With no mode flag, defaults to `-lwc` (lines, words, bytes). Reads stdin when no files are given. When multiple files are provided, a `total` row is appended.

**JSON output:** array of objects with `file`, plus whichever count fields are active.
```json
[{"file": "foo.c", "lines": 10, "words": 50, "bytes": 400},
 {"file": "total", "lines": 10, "words": 50, "bytes": 400}]
```

---

### `cat`

Concatenate files and print to standard output.

```
cat [-h] [-n] [-E] [--json] [FILE...]
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show help and exit |
| `-n`, `--number` | Number all output lines |
| `-E`, `--show-ends` | Display `$` at end of each line |
| `--json` | Emit file contents as JSON |

Reads stdin when no files are given or when `FILE` is `-`. `-n` and `-E` may be combined.

**JSON output:** array of objects with `file` (omitted for stdin) and `content` (full text as a JSON string with escaped newlines).
```json
[{"file": "foo.c", "content": "#include <stdio.h>\n..."}]
```

---

### `pkg` _(to be implemented)_

Local package manager.

```
pkg <subcommand> [OPTIONS]
```

| Subcommand | Description |
|---|---|
| `build` | Package the current directory into a `.tar.gz` using `pkg.json` metadata |
| `install` | Install a local `.tar.gz` package |
| `remove` | Remove an installed package by name |
| `list` | List installed packages |
| `info NAME` | Show metadata for an installed package |
| `publish` | Upload a built package to the online registry (requires `curl`) |
| `search QUERY` | Search the online registry (requires `curl`) |

Each subcommand supports `-h`/`--help` and `--json`.

**`pkg.json` manifest fields:**
```json
{
  "name": "ls",
  "version": "1.0.0",
  "description": "list directory contents",
  "author": "",
  "license": "MIT",
  "files": ["ls", "libls.a"]
}
```

`pkg build` may populate `description` automatically by calling `spec->summary` via the linked `cmd_spec_t` (when built with the shell library) or by running the binary with `--help` and capturing the first line.

---

### `help` _(shell built-in, to be implemented)_

List all registered built-in commands and their summaries, or show full usage for one command.

```
help [COMMAND]
```

With no argument: prints a table of all commands registered in the in-memory registry (name + `summary`).
With `COMMAND`: calls `spec->print_usage(stdout)` for that command.

This command has no standalone binary — it is a shell built-in only.

---

## Agent / MCP integration

All commands with `--json` are designed to be called by an LLM agent or MCP server tool. Rules:

1. `--json` output is always a valid JSON value (object or array) on stdout.
2. Exit code `0` = success, non-zero = error.
3. Error messages go to stderr only; stdout remains clean JSON or empty.
4. Field names are stable snake_case strings.

The shell may expose these commands as MCP tools by wrapping each `cmd_spec_t` — using `summary` as the tool description and capturing `--json` stdout as the tool result.
