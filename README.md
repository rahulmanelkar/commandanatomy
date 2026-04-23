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
│   ├── ls/                 # list directory contents
│   └── stat/               # display file status
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
```

Binaries are produced inside each app's directory (`apps/ls/ls`, `apps/stat/stat`).

## Commands

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

# JSON output for agent/MCP consumption
apps/ls/ls --json /etc
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
apps/stat/stat --json /etc/hostname
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

See `CommandAnatomy.ipynb` for a full walkthrough of the design.
