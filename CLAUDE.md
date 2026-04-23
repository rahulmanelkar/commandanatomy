# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a course project for building a custom Unix shell ecosystem in C. The primary artifact so far is `CommandAnatomy.ipynb`, which serves as both lecture slides and a guided homework notebook. Students use an AI assistant to implement the components described below.

## Intended Architecture

The project defines a standard **app anatomy** for every CLI command in the shell ecosystem:

### `cmd_spec_t` — command specification struct
Each command module must define exactly one `cmd_spec_t` with:
- `name`, `summary`, `long_help` — metadata
- `run(argc, argv)` — main entrypoint (handles parsing + logic via `argtable3`)
- `print_usage(out)` — generates help text from the same `argtable3` definitions

### Key design principle
`argtable3` is the **single source of truth** for CLI parsing, `--help` output, and package documentation. Both `run()` and `print_usage()` call the same internal `build_<name>_argtable()` helper.

### Expected directory structure (not yet present)
- `apps/hello/` — reference implementation of the anatomy (`cmd_spec.h`, `cmd_hello.c`, `hello_main.c`, `registry.c`, `Makefile`)
- `PackageManagement/APPANATOMY.md` — AI refactoring guide for converting existing commands to the anatomy
- `CLItools.md` — required shell commands and flags spec (including `--json` for agent/MCP integration)

### Components to be built
1. **Standalone binaries**: `ls`, `wc`, `cat` refactored into `cmd_spec_t` modules
2. **`pkg` tool** (C): local package manager using `pkg.json` manifests, `tar` for packaging, optional `curl` for an online registry
3. **Shell built-ins**: commands registered via a central in-memory registry; `help` uses the registry and `print_usage`
4. **Node.js registry server**: backend for online package hosting (optional)
5. **LLM natural-language interface**: `@` prefix in the shell triggers an LLM (optional)

## Build conventions (apply when implementing)

- Target: Linux, GCC
- No heavy external libraries — keep it portable
- Each command builds to both a standalone binary and a `.a` static library (for linking into the shell)
- `Makefile` per module; top-level `Makefile` coordinates everything
- `argtable3` is the only approved argument-parsing library

## Prompt history

Every prompt submitted in this project is automatically appended to `prompt_history.md` (via a `UserPromptSubmit` hook in `.claude/settings.json`). Each entry is timestamped in `## [YYYY-MM-DD HH:MM:SS]` format.

## Working with the notebook

The notebook is designed for Google Colab but works in any Jupyter environment. It contains embedded "Prompt to AI" sections — use these as starting points when implementing components. When refactoring existing commands, provide the AI with `APPANATOMY.md` plus the current implementation.
