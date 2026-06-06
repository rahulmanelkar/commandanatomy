# BNF Shell Lab (Session 7)

This is the student-facing lab handout for the grammar-based shell module.

## Where The Lab Lives
- Guided notebook: `notebooks/BNF_Shell_Lab.ipynb`
- Student AI workflow rules: `STUDENT.md`
- Repo guidelines: `AGENTS.md`
- Reference notes: `docs/shell_coding.md`

## Goals
- Understand how a BNF grammar defines a language and drives parsing.
- Generate a parser/AST (BNFC or Flex/Bison) and connect it to an execution layer.
- Extend the shell with environment variables and simple expansion rules.

## What Students Build
Students implement a minimal grammar-based shell that supports:
- command + args
- pipelines
- basic redirection
- background execution (minimal job tracking)

Then they extend it with:
- environment variable assignment
- variable expansion

## Deliverables
- Updated grammar and generated parser (BNFC or Flex/Bison toolchain).
- Execution integration (AST -> pipeline/process graph -> run).
- A small test suite with valid and invalid syntax cases.
- A short AI work log: prompts used, what was accepted/modified, and what was verified.

## Notes
- Keep the grammar minimal and iterate based on failing tests.
- Treat AI output as untrusted until you can build and verify behavior.
- Prefer small, reviewable diffs over large rewrites.

