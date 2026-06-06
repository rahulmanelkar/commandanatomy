# THE RAHULBOX MASTERCLASS
### A Personal Systems-Programming Textbook, Built From Your Own Shell

> **Living document — now complete.** Eleven chapters and two
> appendices, finished 2026-06-05. Still living: ask any question and
> a targeted deep-dive will be inserted where it belongs.
>
> Every line of C quoted in this book is real, extracted verbatim from
> this repository, and cited as `path/to/file.c:line`.

| Status | Date | Event |
|---|---|---|
| ✅ | 2026-06-05 | Document initialized; Table of Contents; **Chapter 1** written |
| ✅ | 2026-06-05 | **Chapter 2** appended — reader-steered: the Forking Mirror jumped the queue; *The Heap* renumbered to Chapter 3 |
| ✅ | 2026-06-05 | **Chapter 3** appended — the Warehouse District; the `realloc` verdict lands on `cat_json` |
| ✅ | 2026-06-05 | **Chapter 4** appended — function pointers, the registry, argtable3 inside-out; absorbed the old Chapter 5, so later chapters renumbered (5–12) |
| ✅ | 2026-06-05 | **Chapter 5** appended — the Speedrunners jumped the queue (tokens → Ch 6, plumbing → Ch 7); the TLS patch in vendored argtable3 gets its biography |
| ✅ | 2026-06-05 | **Chapter 6** appended — the system-integration milestone: Tin-Can sockets + the `@` grammar hook (absorbed the old Ch 10–11; later chapters renumbered) |
| ✅ | 2026-06-05 | **Chapter 7** appended — the lexer's three modes, `$?` injection, the bucket parser; **the audit found a real crash** (trailing `\|` segfaults the shell — §7.5, fix in Lab 5) |
| ✅ | 2026-06-05 | **Chapter 8** appended — the card-box completed; the CLOEXEC deadlock resurrected live (5.002 s hang, rc 124) and the 0664/0644 permission fingerprint discovered |
| ✅ | 2026-06-05 | **Chapter 9** appended — the Cooked Gatekeeper joins the cast; the rulebook swap, the safe-switch protocol, and the honest hole in the atexit net |
| ✅ | 2026-06-05 | **Chapter 10** appended — the decoder's burst-grammar, the switching-Pip digit collector, redraw invariants, and the keystroke-rescue `goto` |
| ✅ | 2026-06-05 | **Chapter 11 (the finale)** + **Appendices A & B** appended — the book is complete: 11 chapters, 2 appendices, rules R1–R63, a six-character cast |
| ✅ | 2026-06-05 | Findings 1–3 **fixed in the codebase** (pipe guards, realloc proxy, `tok.h` comment) — all 58 project tests pass; book citations pin to the pre-fix tree (`ece1997`) |

---

## THE CAST

Four recurring characters illustrate every mechanism in this book. Meet
them once here; they will earn their keep chapter by chapter.

### Pip the Pointer
```
     Pip (a pointer variable)              the pointee
    ┌──────────────────┐                 ┌──────────────┐
    │  0x7ffc51a3bd80  │ ●━━━string━━━━▶ │  'c' 'a' 't' │
    └──────────────────┘                 └──────────────┘
     8 bytes wide: holds                  the actual data,
     an ADDRESS, nothing else             living somewhere else
```
Pip is a small worker who carries one thing: a **string tied to a
physical memory box**. Pip's own pocket is exactly 8 bytes (on x86-64) —
just big enough to write down one address. Pip never carries the data;
Pip carries *directions to* the data.

- **Tying the string** = assignment: `p = &x;`
- **Walking the string** = dereference: `*p`
- **A string tied to nothing** = `NULL` (address 0 — a fenced-off crater;
  walking that string is a segfault)
- **Pip wearing a blank name-tag** = `void *` (an address with no record
  of what kind of box is at the other end)
- **Pip-on-Pip** = `char **`, `struct arg_lit **` — a string tied to a
  box *that itself contains another Pip*

### The Forking Mirror *(stars in Chapter 2)*
A magical cloning device. When `mysh` aims it at its own room and
flashes — `fork()` — the **entire room is duplicated**: every box, every
shelf, every Pip and every string Pip holds, copied into an isolated
parallel dimension. The clone can trash its copy of the room and the
original never notices. That isolation is *the* reason your shell can run
`execvp()` in the child without destroying itself.

### The Threaddy Speedrunners *(star in Chapter 5)*
A crew of lightning-fast workers who — unlike the Mirror's clones — all
work **inside the same single room**, sharing every shelf, table, and box
(the address space). Each Speedrunner carries only one private thing: a
personal **note-pad** (their stack). Your `run_pipeline()` hires one
Speedrunner per pipeline stage (`pthread_create`, `shell/mysh.c:394`),
which is why `cat notes.txt | grep x | wc -l` runs as three workers in
one process instead of three cloned rooms.

### The Tin-Can Socket Timers *(star in Chapter 6)*
Tin-can telephones strung between rooms across the world (TCP
connections), each fitted with a strict **5-second self-destruct
countdown**. Your `fetch` command refuses to wait forever on a dead
line: `connect_timeout()` arms the can with `poll(&pfd, 1, timeout_secs
* 1000)` (`apps/fetch/cmd_fetch.c:122`) and `setsockopt(fd, SOL_SOCKET,
SO_RCVTIMEO, ...)` (`apps/fetch/cmd_fetch.c:310`) arms the listening
side. When the countdown hits zero, the worker hangs up and reports
`ETIMEDOUT` instead of standing there for eternity.

### Supporting cast
- **The Warehouse Clerk** *(Chapter 3)* — glibc's allocator: rents
  boxes, keeps his ledger entry just before each one, and never erases
  what a departing tenant leaves behind.
- **The Cooked Gatekeeper** *(Chapter 9)* — the kernel's tty line
  discipline: sits between the keyboard and your `read()`, echoing,
  buffering, and editing keystrokes at his desk, releasing tidy lines
  only on Enter — until raw mode sends him on break.

---

## TABLE OF CONTENTS

*Chapter numbers follow our actual reading order — the book renumbers
itself when you steer. (Reader steers so far: the Mirror to Ch 2; the
anatomy chapters merged into Ch 4; the Speedrunners to Ch 5; the
Tin-Cans and the `@` hook merged into Ch 6.)*

| # | Chapter | Theme | Cast lead | Anchored in | Status |
|---|---------|-------|-----------|-------------|--------|
| 1 | **Pointers, Memory Layout, and the Stack Frame Architecture** | Memory | Pip | `cmd_spec.h`, `cmd_cat.c`, `cmd_sort.c`, `mysh.c`, `linedit.c` | ✅ |
| 2 | **Process Isolation, Mirror Machines, and the stdin Contamination Fix** | Processes | The Forking Mirror | `shell/mysh.c` | ✅ |
| 3 | **The Heap: Pip's Warehouse District, Dynamic Buffers, and Valgrind Safeguards** | Memory | Pip | `cat_json`, `read_all_lines`, fetch's reply accumulator | ✅ |
| 4 | **Function Pointers, Central Registries, and the Inside-Out Anatomy of argtable3** *(absorbed the old Ch 5)* | Anatomy | Pip | `cmd_spec.h`, the registry, `spec->run(...)`, every `build_*_argtable()` | ✅ |
| 5 | **The Threaddy Speedrunners: POSIX Threads, Concurrent Streams, and Thread-Local Storage** | Concurrency | Threaddies | `run_pipeline()`, `stage_thread_fn()`, the `vendor/argtable3` TLS patch | ✅ |
| 6 | **The Tin-Can Socket Timers: Non-Blocking TCP Sockets and the Grammatical AI Hook** *(absorbed the old Ch 10–11)* | Network | Tin-Cans | `connect_timeout()`, the recv loop, `run_at_prompt()` | ✅ |
| 7 | **Strings, Tokens, and the Pipeline Parser State Machine** | Shell core | Pip | `tok.c`, `parse_pipeline()`, `stage_t` | ✅ |
| 8 | **Plumbing: File Descriptor Card-Boxes, dup2 Overwrites, and the CLOEXEC Self-Destruct** | Shell core | Mirror & Threaddies | `apply_redirs()`, `run_pipeline()` fd choreography | ✅ |
| 9 | **Terminal Wrestling: Taming the Keyboard with Termios Raw Mode** | Terminal | The Cooked Gatekeeper | `raw_enable()`, `TCSADRAIN`, `atexit` | ✅ |
| 10 | **The Line Editor: Custom Key Maps, Escape Sequence Decoding, and Horizontal Redraw Loops** | Terminal | Pip | `le_edit()`, `le_escape()`, `le_refresh()` | ✅ |
| 11 | **The Life of a Keystroke** — one command traced through every layer | Synthesis | full cast | everything | ✅ |
| A | **Appendix A: The Cast Character Glossary** | Reference | — | — | ✅ |
| B | **Appendix B: The System Call and libc Primitive Index** | Reference | — | — | ✅ |

---
---

# CHAPTER 1
# Pointers, Memory Layout, and the Stack Frame Architecture

> *In which Pip the Pointer gives you a tour of the room your shell
> lives in, demonstrates why `build_cat_argtable()` takes seven
> double-pointers, and graduates you with the single hardest line of C
> in your repository:*
> `*(const char * const *)a`

---

## 1.1 The Room of Boxes — your process's virtual address space

Before a single pointer makes sense, you need the map of the territory.

When you run `./shell/mysh`, the Linux kernel builds your process a
private **virtual address space**: a colossal room of numbered boxes,
one box per byte, with addresses running from 0 up to 2⁴⁷ and beyond.
*Private* is the key word — every process gets its own room, and one
room cannot reach into another. (Hold that thought: when the **Forking
Mirror** flashes in Chapter 2, what gets duplicated is exactly this
room, in its entirety. That's what "process isolation" *is*.)

The room is not uniform. It is divided into **districts**, each with
different rules. Here is the actual map of a running `mysh`, with your
own symbols placed where they really live:

```
 high addresses
 ┌──────────────────────────────────────────────────────────────┐ 0x7ffc────
 │  STACK  ("the tower of call trays" — grows DOWNWARD ↓)       │
 │    main():     char line[4096]          mysh.c:591           │
 │                stage_t stages[16]       mysh.c:675  (~17 KB) │
 │                tok_t tok                mysh.c:661  (~1 KB)  │
 │    cat_run():  void *tbl[7]             cmd_cat.c:151        │
 │    sort_run(): sort_ctx_t ctx           cmd_sort.c:196       │
 ├──────────────────────────────────────────────────────────────┤
 │           (huge unused gap — the safety moat)                │
 ├──────────────────────────────────────────────────────────────┤ 0x7f──────
 │  MMAP DISTRICT                                               │
 │    libc.so, libpthread machinery, big allocations,           │
 │    and — later — each Threaddy Speedrunner's private         │
 │    8 MB note-pad (thread stacks live HERE, not in STACK)     │
 ├──────────────────────────────────────────────────────────────┤
 │           (gap)                                              │
 ├──────────────────────────────────────────────────────────────┤
 │  HEAP  ("the warehouse district" — grows UPWARD ↑)           │
 │    every token string strndup'd by tok_split    (tok.c)      │
 │    every getline() buffer                cmd_cat.c:67        │
 │    every argtable handle made by arg_lit0/arg_filen          │
 │    every stage_arg_t packet              mysh.c:363          │
 │    fetch's growable reply buffer         cmd_fetch.c:371     │
 ├──────────────────────────────────────────────────────────────┤ 0x5555────
 │  .bss   (globals that start as ZERO — stored as a promise,   │
 │          not as bytes in the binary)                         │
 │    static cmd_spec_t *registry[64];      mysh.c:73           │
 │    static int registry_n;                mysh.c:74           │
 ├──────────────────────────────────────────────────────────────┤
 │  .data  (globals with a nonzero starting value)              │
 │    cmd_spec_t cmd_cat_spec = {...};      cmd_cat.c:214       │
 │    static int g_cmd_fd = -1;             mysh.c:68           │
 ├──────────────────────────────────────────────────────────────┤
 │  .rodata  (READ-ONLY: string literals, const tables)         │
 │    "cat"   "concatenate files and print to standard output"  │
 │    "mysh [%d]> "   "fetch: %s: %s\n"                         │
 ├──────────────────────────────────────────────────────────────┤
 │  .text   (the CODE itself, also read-only)                   │
 │    the machine instructions of cat_run, le_edit, main, ...   │
 └──────────────────────────────────────────────────────────────┘ low addresses
```

Five facts about this map that the rest of the book stands on:

1. **An address is just a number.** Box #140,732,121,234,816 — usually
   written in hex, `0x7ffc51a3bd80`. A pointer is a variable whose
   *value* is one of these numbers. That's the whole secret. Everything
   else is bookkeeping about *which district* the number points into and
   *how long* the box at that number stays valid.

2. **The districts have different lifetimes.** `.text`/`.rodata`/`.data`/
   `.bss` boxes live as long as the process. STACK boxes live as long as
   one function call. HEAP boxes live from `malloc` until `free` — *you*
   decide. Ninety percent of C bugs are a Pip whose string is tied into
   a district whose box has already been demolished.

3. **The districts have different permissions.** Write into `.rodata`
   and the kernel kills you with SIGSEGV. This is why `cmd_spec.h:7`
   declares `const char *name;` — the `"cat"` that `name` points at is
   carved into the read-only district, and `const` makes the compiler
   stop you *before* the kernel has to.

4. **Addresses are randomized per run** (ASLR — address-space layout
   randomization). Run the lab in §1.11 twice and the numbers shift; the
   *ordering* of districts holds.

5. **A pointer is 8 bytes, no matter what it points at.** `char *`,
   `FILE *`, `cmd_spec_t *`, `void *`, even a pointer to a function —
   all 8 bytes on x86-64, because all of them hold one address. Pip's
   pocket is one size.

---

## 1.2 Meet Pip — declaration, `&`, and `*`

Take the smallest real pointer in your repository, from the struct that
defines your entire command anatomy:

```c
typedef struct cmd_spec {
    const char *name;
```
*(`include/cmd_spec.h:6-7`)*

Read `const char *name` from the inside out: **`name` is a pointer to a
`char` that is `const`**. Concretely, `name` is one 8-byte box. When
`cmd_cat.c:215` initializes it —

```c
cmd_spec_t cmd_cat_spec = {
    .name       = "cat",
```
*(`apps/cat/cmd_cat.c:214-215`)*

— the compiler places the three bytes `'c' 'a' 't'` (plus a
terminating `'\0'`) into the `.rodata` district, and writes *that
address* into the `name` box, which itself sits in `.data`:

```
        .data district                      .rodata district
   ┌─────────────────────────┐         ┌───────────────────────┐
   │ cmd_cat_spec            │         │                       │
   │  .name ●━━━━━━━━━━━━━━━━┿━━━━━━━━▶│ 'c' 'a' 't' '\0'      │
   │  .summary ●━━━━━━━━━━━━━┿━━━━━━━━▶│ 'c' 'o' 'n' 'c' ...   │
   │  ...                    │         │                       │
   └─────────────────────────┘         └───────────────────────┘
```

Pip's two verbs, which you will use ten thousand times:

| Syntax | Pip's action | Plain English |
|---|---|---|
| `&x` | *"Tell me the number on x's box."* | take the **address of** `x` |
| `*p` | *"Walk the string; act on the box at its end."* | **dereference** `p` |

And one symmetry worth tattooing somewhere: **declaration mirrors use.**
`char *p` literally says "`*p` is a `char`" — if you walk the string,
you find a char. This single idea will decode every gnarly declaration
in this book, including the function pointer `int (*run)(int, char **,
FILE *, FILE *)` ("`(*run)(...)` is a call returning `int`" — Chapter
4's opening act).

Two special Pips:

- **`NULL`** — the string tied to nothing, used as an honest "no such
  thing" signal. Your registry uses it as exactly that:

  ```c
  static cmd_spec_t *reg_find(const char *name)
  {
      for (int i = 0; i < registry_n; i++)
          if (strcmp(registry[i]->name, name) == 0) return registry[i];
      return NULL;
  }
  ```
  *(`shell/mysh.c:81-86`)*

  Every caller checks before walking: `if (spec) { ... }`
  (`shell/mysh.c:201`). Walking a NULL string — `spec->run` when `spec`
  is NULL — dereferences address 0, and the kernel keeps that district
  permanently unmapped *precisely so that this common bug crashes
  loudly* instead of corrupting data silently.

- **`void *`** — Pip with a blank name-tag. The address is real, but
  the type information is erased, so the compiler won't let you walk
  the string until you re-label it with a cast. Your argtables are
  built from exactly these:

  ```c
  void            *tbl[7];
  ```
  *(`apps/cat/cmd_cat.c:151`)*

  Seven blank-tag Pips in a row — necessary because slot 0 holds a
  `struct arg_lit *`, slot 4 a `struct arg_file *`, slot 5 a
  `struct arg_end *`: different types, one array. Blank tags are the
  only way C lets one shelf hold them all. (argtable3 re-labels them
  internally; the price of `void *` is that *somebody* must remember
  the true types. §1.9 shows the same trick inside `qsort_r`.)

---

## 1.3 `char **argv` — Pip's family tree

The most important double-Pip in Unix is the one every command in your
anatomy receives:

```c
int  (*run)(int argc, char **argv, FILE *in_stream, FILE *out_stream);
```
*(`include/cmd_spec.h:10`)*

`char **argv`: **a pointer to a pointer to char**. Pip holding a string
to a shelf of more Pips, each of *those* holding a string to actual
text. Here is the real, complete picture for `cat -n notes.txt` typed
at your `mysh>` prompt:

```
 STACK (main's tray)                 HEAP (strndup'd by tok_split)
 ┌──────────────────────────┐
 │ stages[0].argv            │
 │   [0] ●━━━━━━━━━━━━━━━━━━┿━━━━▶ "cat\0"
 │   [1] ●━━━━━━━━━━━━━━━━━━┿━━━━▶ "-n\0"
 │   [2] ●━━━━━━━━━━━━━━━━━━┿━━━━▶ "notes.txt\0"
 │   [3] = NULL  ◀── the fence post                  argc == 3
 │   ...                     │
 └──────────────────────────┘
```

Three deep truths hiding in this little diagram:

**(a) The NULL fence post is a load-bearing contract.** Your parser
plants it deliberately, twice:

```c
        if (strcmp(w, "|") == 0) {
            cur->argv[cur->argc] = NULL;
```
*(`shell/mysh.c:120-121`)*
```c
    cur->argv[cur->argc] = NULL;
```
*(`shell/mysh.c:151`)*

Why? Because `execvp(s->argv[0], s->argv)` (`shell/mysh.c:244`) hands
the shelf to the kernel, and the kernel walks slot after slot **until it
hits NULL** — there is no `argc` parameter in `execvp`'s signature at
all. Forget the fence post and the kernel marches off the end of the
shelf into garbage. `argv[argc] == NULL` is the oldest handshake in
Unix, and your shell performs it correctly.

**(b) The same shelf serves two masters.** When the command is
*external* (`/bin/date`), the shelf goes to `execvp` and the kernel
*copies* all the strings into the brand-new process's room. But when the
command is *internal* (`cat`, found in your registry), the very same
shelf is passed, strings untouched, straight into your own function:

```c
        int rc = spec->run(s->argc, s->argv, in, out);
```
*(`shell/mysh.c:225`)*

So when `cat_run(argc, argv, ...)` executes, its `argv[2]` is literally
a string into the heap box that `tok_split` allocated minutes earlier.
No copy. This is the efficiency superpower of your in-process design —
and it creates a lifetime obligation we'll meet in §1.10.

**(c) `**` is not exotic — it's just two hops.** `argv[1][0]` means:
walk Pip's string to the shelf, step to slot 1, walk *that* Pip's
string, read byte 0 → `'-'`. Every level of `*` in a type is one
string-walk at runtime.

---

## 1.4 Pointer arithmetic — walking the shelf

Pointers can be added to. But — crucial subtlety — **C scales the
arithmetic by the size of the thing pointed at.** `p + 1` means "one
*element* over", not "one *byte* over".

Exhibit one, bytes. Your line editor inserts a character mid-line by
shoving the tail one slot right:

```c
static void le_insert(le_t *l, unsigned char c)
{
    if (l->len + 1 >= l->bufsz) return;          /* line full: drop the key */
    memmove(l->buf + l->pos + 1, l->buf + l->pos, l->len - l->pos);
    l->buf[l->pos] = (char)c;
```
*(`shell/linedit.c:198-202`)*

`l->buf` is a `char *`, so `l->buf + l->pos` advances by `pos` **bytes**
— Pip side-steps `pos` one-byte boxes down the row, and `memmove` slides
`len - pos` of them one box rightward to open a gap. (Why `memmove`
rather than `memcpy`? The source and destination *overlap*; `memmove`
guarantees correctness in that case. Your code chose right.)

Exhibit two, eight-byte strides. Your `sort` appends a freshly read
batch of line-pointers onto its master shelf:

```c
        memcpy(lines + total, batch, n * sizeof(char *)); \
```
*(`apps/sort/cmd_sort.c:229`)*

`lines` is a `char **` — a shelf of Pips — so `lines + total` advances
by `total × 8` bytes. Same `+` in the source code, eight times the
distance in the room. The compiler does the multiplication because it
knows the element type; this is why Pip's name-tag (the pointer's type)
matters even though every Pip's pocket is the same size.

One more spelling you'll see everywhere: **`p[i]` is defined as
`*(p + i)`**. Array indexing *is* pointer arithmetic with a walk at the
end. The two notations are the same machine code wearing different
clothes.

---

## 1.5 The Stack — a tower of call trays

Now the architecture half of this chapter's title.

Every time a function is called, the CPU stacks a new **tray** (the
real term: **stack frame**) on top of the tower: a slab of memory
holding, conceptually,

```
   ┌─────────────────────────────────┐
   │ the RETURN ADDRESS              │ ← where in .text to resume when done
   │ saved registers                 │
   │ the function's LOCAL VARIABLES  │ ← every non-static local lives here*
   └─────────────────────────────────┘
                       *unless the optimizer keeps it in a register
```

When the function returns, its tray is **removed and considered
demolished** — not actively erased, just declared invalid, ready to be
overwritten by the next call. The tower grows *downward* in addresses
(see the map in §1.1) and is finite: commonly 8 MiB (`ulimit -s` →
`8192` KiB on most Linux boxes). LIFO, automatic, fast — allocation is
literally "move the stack pointer", a single instruction.

Let's freeze your shell at the deepest interesting moment and look at
the actual tower. You type `cat -n notes.txt`; this is a single-stage
pipeline of an internal command, so the call chain is `main →
run_pipeline → run_inproc → cat_run → build_cat_argtable`:

```
        THE TOWER (stack district, growing downward ↓)

 ┌ main ──────────────────────────────────────────────┐
 │  char line[4096]              ← the raw typed line │  ~4 KB
 │  tok_t tok                    ← 128 Pip slots + n  │  ~1 KB
 │  stage_t stages[16]           ← parsed pipeline    │  ~17 KB
 └────────────────────────────────────────────────────┘
 ┌ run_pipeline ───────────────────────────────────────┐
 │  (nstages == 1 → returns run_inproc(&stages[0])     │  thin tray
 │   immediately; mysh.c:270)                          │
 └─────────────────────────────────────────────────────┘
 ┌ run_inproc ────────────────────────────────────────┐
 │  FILE *in, *out, *owned_in, *owned_out             │  4 Pips
 └────────────────────────────────────────────────────┘
 ┌ cat_run ───────────────────────────────────────────┐
 │  struct arg_lit  *help, *number, *show_ends, *json │  4 Pips   ┐ all six tied
 │  struct arg_file *files;  struct arg_end *end      │  2 Pips   ┘ into the HEAP
 │  void *tbl[7]                                      │  7 blank-tag Pips
 │  int nerrors, use_number, ... ret                  │
 └────────────────────────────────────────────────────┘
 ┌ build_cat_argtable ────────────────────────────────┐
 │  parameters: help, number, show_ends, json,        │  each one a string
 │              files, end, tbl                       │  tied UP into
 └────────────────────────────────────────────────────┘  cat_run's tray ↑
```

Three load-bearing observations:

**(a) Your shell's main loop carries ~22 KB of tray.** `stage_t` is an
array of 128 `char *` (1024 bytes) plus redirection fields — 1056 bytes
after padding — and `stages[PIPELINE_MAX]` is 16 of those: 16,896
bytes, allocated by *moving one register*. This is the stack's gift:
zero-cost allocation, automatic cleanup. Its price: fixed size, and a
lifetime you don't control.

**(b) The arg handles live in the heap; only the Pips are on the
tray.** When `build_cat_argtable` runs `arg_lit0("h", "help", ...)`,
argtable3 `malloc`s a `struct arg_lit` in the warehouse district and
returns its address. The struct *outlives* `build_cat_argtable`'s tray
precisely because it's heap-allocated; the tray only ever held the
8-byte directions to it. Stack-holds-the-Pip, heap-holds-the-thing is
the default geometry of serious C programs.

**(c) The bottom tray writes into the tray above it.** That's the next
section — the most important calling convention in your entire
codebase.

---

## 1.6 Out-parameters — how `build_cat_argtable` returns seven things

C functions return **one** value. Your argtable builder needs to hand
back **seven** (six typed handles plus a filled table). Look at its
signature with fresh eyes:

```c
static void build_cat_argtable(
    struct arg_lit  **help,
    struct arg_lit  **number,
    struct arg_lit  **show_ends,
    struct arg_lit  **json,
    struct arg_file **files,
    struct arg_end  **end,
    void            **tbl)        /* caller-allocated array of 7 slots */
```
*(`apps/cat/cmd_cat.c:12-19`)*

Every parameter is a **double-Pip**, and here's why that is forced, not
fancy. C passes arguments **by value** — the callee receives
*photocopies*. If the signature were `struct arg_lit *help`, then
`build_cat_argtable` would get a photocopy of cat_run's `help` Pip;
re-tying the *photocopy's* string (`help = arg_lit0(...)`) would change
nothing in `cat_run`'s tray, and the work would evaporate when the
builder's tray is demolished.

The fix: don't send a copy of the Pip — **send the address of the Pip's
own box**. The caller does this:

```c
    struct arg_lit  *help, *number, *show_ends, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void            *tbl[7];

    build_cat_argtable(&help, &number, &show_ends, &json, &files, &end, tbl);
```
*(`apps/cat/cmd_cat.c:148-153`)*

and the callee reaches *up* the tower and writes into the caller's tray:

```c
    *help      = arg_lit0("h", "help",      "show this help and exit");
```
*(`apps/cat/cmd_cat.c:21`)*

`*help = ...` — "walk the string up into cat_run's tray; re-tie the Pip
you find there to this new heap box." Pictorially:

```
 ┌ cat_run's tray ───────────────────────────┐         HEAP
 │  help ●━━━━━━(re-tied by callee)━━━━━━━━━━┿━━━━▶ struct arg_lit
 │   ▲                                       │      {"h","help",...}
 └───┃───────────────────────────────────────┘
 ┌───┃ build_cat_argtable's tray ────────────┐
 │  help ●━━━┛  (parameter: the ADDRESS of   │
 │              the box one tray up)         │
 └───────────────────────────────────────────┘
```

This is the **out-parameter pattern**, and it is the skeleton key to
your codebase — `cat`, `sort`, `fetch`, `ls`, all fifteen apps build
their argtables this way. It's also *why* the same builder can serve two
masters, which is your project's stated design principle ("argtable3 is
the single source of truth"): `cat_run` (`cmd_cat.c:153`) and
`cat_print_usage` (`cmd_cat.c:132`) each declare their own tray of
Pips, pass their addresses to the *same* builder, and get identical
tables — parsing and `--help` can never drift apart, because they're
generated from one function.

Note `tbl` is passed *without* `&`. Why? Because of…

### Array decay — and the disappearing `sizeof`

When an array's name is used in an expression, it **decays** into a
Pip tied to its first element. `tbl` (an array of 7 `void *`) becomes a
`void **` automatically; `stages` becomes a `stage_t *`; that's also why
`run_inproc(&stages[0])` and `run_inproc(stages)` would be the same
call.

Decay has a famous casualty: **the size is forgotten.** Inside the
callee, the parameter is just a Pip — the compiler no longer knows the
shelf had 7 slots or 4096 bytes. Your code handles this with textbook
discipline. Watch the hand-off into the line editor:

```c
            if (linedit_read(input, prompt, line, sizeof line) < 0) {
```
*(`shell/mysh.c:607`)*

In `main`, `line` is a real array, so `sizeof line` is **4096**. But
inside the callee —

```c
int linedit_read(FILE *in, const char *prompt, char *buf, size_t bufsz)
```
*(`shell/linedit.c:445`)*

— `buf` is a mere Pip, and `sizeof buf` would be **8**. The size cannot
survive the call, *so it travels alongside as a second parameter*,
`bufsz`. Every honest C API does this (compare `getline`'s `&cap`,
`fgets`'s `int size`, `snprintf`'s `size_t size`). When you see a
pointer parameter without a size companion, either the type has a fixed
size, the data carries its own terminator (`'\0'`, the NULL fence
post), or someone has written a buffer overflow.

The editor then bundles Pip *plus* both lengths into its state struct —

```c
typedef struct {
    int         ifd, ofd;   /* tty file descriptors                          */
    char       *buf;        /* caller's line buffer                          */
    size_t      bufsz;      /* its capacity (including the NUL)              */
    size_t      len;        /* current line length                           */
    size_t      pos;        /* cursor index into buf, 0..len                 */
```
*(`shell/linedit.c:151-156`)*

— the *pointer / capacity / length* trio that every growable or bounded
buffer in existence carries, formally or informally. Memorize the trio;
Chapter 3 is largely about what happens when `len` wants to exceed
`bufsz`.

And notice what `le_edit(&l)` then does (`shell/linedit.c:467`): the
whole editor state — five fields and two Pips — is passed **by
reference** as a single 8-byte address, and every keystroke handler
(`le_insert(l, c)`, `le_backspace(l)`) edits *the caller's* buffer
in place through it. When `linedit_read` returns, `main`'s `line[]`
already contains the finished command. No copies were made at any point
between your keystroke and the parser. That is what "passing tables by
reference" buys.

---

## 1.7 First contact with the Heap — why `getline` needs `&line`

Full heap treatment is Chapter 3, but one of its Pips is too good to
postpone, because it's the *same lesson as §1.6 wearing a different
hat*. Here is your `cat`'s inner loop:

```c
static int cat_plain(FILE *fp, FILE *out, int number, int show_ends)
{
    char   *line = NULL;
    size_t  cap  = 0;
    ssize_t len;
    long long lineno = 0;

    while ((len = getline(&line, &cap, fp)) != -1) {
```
*(`apps/cat/cmd_cat.c:60-67`)*

Why `&line` — the address of the Pip — and not `line`?

Because `getline` *manages the buffer for you*. On the first call,
`line` is NULL and `getline` mallocs a buffer in the warehouse
district. On a later call, if a longer line arrives than `cap` allows,
`getline` calls `realloc` — and `realloc` is allowed to **move the
whole box to a new address**. After a move, the old address is poison.
So `getline` must be able to *re-tie your Pip's string* to wherever the
box now lives — which means it needs the address of the Pip itself.
`char *line` + `&line` = `char **`. Pip-on-Pip, again — §1.6's
out-parameter, §1.3's argv, and this, are all the same single idea:

> **To let a callee change a thing, pass the address of the thing.
> When the thing is itself a pointer, that makes a `**`.**

The trio appears again, naturally: `line` (pointer), `cap` (capacity),
`len` (length of this line). And exactly one `free(line)` after the
loop (`cmd_cat.c:82`) settles the account — `getline` may have
realloc'd the box five times during the loop, but at any instant there
is exactly *one* live box, and your Pip holds its current address.

---

## 1.8 Passing tables by reference — the `parse_pipeline` taxonomy

The chapter's title promised stack frames *and* table-passing. Your
parser's signature is a one-line course in C's three parameter
intents:

```c
static int parse_pipeline(tok_t *tok, stage_t *stages, int *nstages)
```
*(`shell/mysh.c:108`)*

| Parameter | Intent | Why a pointer? |
|---|---|---|
| `tok_t *tok` | **in** — read-only input table | `tok_t` is ~1 KB (128 Pips + a count, `shell/tok.h:6-9`); copying it per call would be waste. Pass 8 bytes of directions instead. |
| `stage_t *stages` | **out** — array the callee fills | The caller owns the storage (its tray); the callee populates it through the string. |
| `int *nstages` | **out** — a scalar result | The single `return` slot is already spent on the error code (`0`/`-1`), so the count travels through a Pip. |

This taxonomy — *in by pointer-to-const-or-big-thing, out by pointer,
error code in the return* — is the native idiom of C APIs, and your
shell speaks it fluently. Compare `resolve_host(const char *host, int
port, struct addrinfo **res, const char **errmsg)` in
`apps/fetch/cmd_fetch.c:76-77`: two ins, two outs (one of them a
Pip-on-Pip, because the "thing being returned" is itself a pointer to a
kernel-built list).

Inside the parser, one idiom deserves a frame of its own:

```c
    stage_t *cur = &stages[0];
    memset(cur, 0, sizeof *cur);
```
*(`shell/mysh.c:113-114`)*

`sizeof *cur` — "the size of whatever Pip's string is tied to" — is
self-maintaining: if `stage_t` grows a field next month, this line is
still correct, whereas `sizeof(stage_t)` written elsewhere risks
drifting if the type of `cur` ever changes. The same defensive spelling
appears at the single most important `malloc` in your shell:
`stage_arg_t *a = malloc(sizeof *a);` (`shell/mysh.c:363`). Small
habit, professional fingerprint.

Finally, registration — passing a *global* table by reference:

```c
    reg_register(&cmd_cat_spec);
```
*(`shell/mysh.c:538`)*

```c
static cmd_spec_t *registry[REGISTRY_MAX];
```
*(`shell/mysh.c:73`)*

The registry is an array of 64 Pips in `.bss`, each tied to a
`cmd_spec_t` in `.data`. Nothing is copied at registration — your whole
command system is 64 strings pointing at structs that were carved into
the binary at compile time. And note that `reg_find` *returning*
`registry[i]` (§1.2) is perfectly safe **because the pointee is a
global**: `.data` boxes outlive every tray. Contrast the cardinal sin:

```c
/* THE CLASSIC BUG — not in your code, and now you know why */
char *bad(void) {
    char tmp[64];
    snprintf(tmp, sizeof tmp, "...");
    return tmp;        /* returning a string tied into MY OWN tray —    */
}                      /* which is demolished by this very `return`.    */
                       /* GCC: "function returns address of local variable" */
```

Returning a pointer is fine; returning a pointer *into your own tray*
is never fine. Lifetime, not syntax, is what separates them — which
brings us to the graduation exam.

---

## 1.9 Graduation — `qsort_r` and `*(const char * const *)a`

The hardest pointer line in your repository lives in `sort`, and after
this section it will look obvious. The setup: `sort_run` has gathered
every input line onto one heap shelf —

```c
    char **lines = NULL;
```
*(`apps/sort/cmd_sort.c:208`)* — a shelf of Pips, each tied to one
heap-allocated line — and built a small descriptor of *how* to sort, on
its own tray:

```c
    sort_ctx_t ctx = {
        .reverse = reverse->count > 0,
        .numeric = numeric->count > 0,
        .key     = key->count > 0 ? key->ival[0] : 0,
        .sep     = (sep->count > 0 && sep->sval[0][0]) ? sep->sval[0][0] : '\0',
    };
```
*(`apps/sort/cmd_sort.c:196-201`)*

Then the call:

```c
    qsort_r(lines, total, sizeof(char *), sort_cmp, &ctx);
```
*(`apps/sort/cmd_sort.c:250`)*

`qsort_r` is *generic* — it sorts shelves of anything: ints, structs,
Pips. To pull that off it works entirely in blank name-tags: "here is a
shelf at `lines`, holding `total` elements of `sizeof(char *)` = 8
bytes each; when you need to compare two, call `sort_cmp` and I'll hand
you their *positions*." Positions — not values. The comparator receives
**the addresses of two slots on the shelf**, as `const void *` because
qsort genuinely doesn't know what's in them:

```c
static int sort_cmp(const void *a, const void *b, void *vctx)
{
    const sort_ctx_t *ctx = vctx;
    const char *la = *(const char * const *)a;
    const char *lb = *(const char * const *)b;
```
*(`apps/sort/cmd_sort.c:98-102`)*

Decode `*(const char * const *)a` in three moves, right to left:

1. **What is `a`, really?** A slot-address. The slots hold `char *`.
   So `a` is secretly a `char **` wearing a blank tag.
2. **The cast** `(const char * const *)a` re-labels it honestly:
   "pointer to (a `const`-qualified pointer to `const char`)". Two
   `const`s, two promises: the *inner* one ("…`char` is const") — I
   won't edit the text of the line; the *middle* one ("`* const`") — I
   won't re-tie the Pip sitting in the slot. The comparator gets
   read-only access at *both* hops.
3. **The leading `*`** walks one hop: from slot-address to slot
   *contents* — and the contents are the Pip tied to the actual string.

```
   a (const void * — blank tag)        the shelf `lines` (HEAP)
      │                               ┌─────────────┐
      └━━━━ secretly &lines[3] ━━━━━▶ │ lines[3] ●──┼───▶ "banana\n"
                                      ├─────────────┤
        *(const char* const*)a  ══▶   │ lines[4] ●──┼───▶ "apple\n"
            == lines[3]               └─────────────┘
            == Pip to "banana\n"
```

One hop (`*a`) lands on the Pip; the string itself is at hop two
(`get_field(la, ctx)` walks it). If you ever write `qsort` over a shelf
of structs instead of Pips, the comparator does only *one* hop — the
double-hop here exists **because the elements are themselves
pointers**. That's the entire mystery.

And the fifth argument? `&ctx` — §1.8's pass-by-reference, smuggling
your tray-resident settings into every comparator call through
`void *vctx`. Re-labeled on arrival (`const sort_ctx_t *ctx = vctx;`),
consulted at the end (`return ctx->reverse ? -r : r;`,
`cmd_sort.c:114`). This is the **context-pointer pattern**: how C
threads *state* through a callback interface that only has room for a
single `void *`. (The non-`_r` `qsort` lacks that slot — its
comparators must read globals, which breaks the moment two threads sort
at once. Your code reached for the reentrant version. Remember this
choice when the Threaddy Speedrunners show up.)

**Why is `&ctx` safe here, when §1.8 just forbade escaping trays?**
Because `qsort_r` *returns before `sort_run` does* — the borrowed
string is only ever walked while the lending tray is still on the
tower. Lifetimes nest; the borrow is legal. Now watch your shell face
the *opposite* case and make the opposite choice:

```c
            stage_arg_t *a = malloc(sizeof *a);
```
*(`shell/mysh.c:363`)*

The per-stage argument packet for a pipeline thread is **deliberately
heap-allocated** — because a Speedrunner (`pthread_create`,
`mysh.c:394`) runs *concurrently with and possibly beyond* the moment
the spawning loop's locals change or vanish. Same pattern (hand a
worker a `void *` of context), opposite lifetime, opposite district.
The author of these two lines understood §1.10 — and by next page, so
will you.

---

## 1.10 Lifetimes and dangling strings — the rules of the demolition site

A pointer is only as good as the box at the end of its string. The
compiler tracks types; **lifetimes are tracked by you**. The three laws:

1. **Passing strings DOWN the tower is always safe.** A callee's tray
   is demolished before its caller's — so `le_edit(&l)`,
   `parse_pipeline(&tok, stages, &nstages)`, `qsort_r(..., &ctx)` are
   all unconditionally sound. (Every example in this chapter was this
   case. That was deliberate.)
2. **Returning a string into your own tray is always a bug.** §1.8's
   cardinal sin. Return strings into the heap, into globals, or into
   *your caller's* tray (out-parameters) instead.
3. **Storing a string SIDEWAYS — into anything that outlives the tray —
   requires the heap.** A struct that lives past the call, a global, or
   a worker on another schedule (a thread!) must not hold tray
   addresses.

Your shell documents a Law-3 borrow *in the source itself* — the most
instructive comment in the repository:

```c
typedef struct {
    cmd_spec_t *spec;
    int         argc;
    char      **argv;        /* points into tok_t; valid while pipeline runs */
```
*(`shell/mysh.c:177-180`)*

The thread packet's `argv` Pips are tied into `tok_t`'s heap strings —
a borrow, not an ownership transfer. What makes it legal is
**sequencing**, visible right in `main`'s loop: `run_pipeline(stages,
nstages)` (`mysh.c:767`) does not return until it has **joined every
worker** —

```c
        if (spawned[i].is_thread) {
            pthread_join(spawned[i].tid, NULL);
```
*(`shell/mysh.c:459-460`)*

— and only after it returns does `main` demolish the boxes:

```c
        tok_free(&tok);
```
*(`shell/mysh.c:769`)*

Reverse those two lines and every running stage's `argv` becomes a fan
of strings into freed warehouse boxes — the **dangling pointer**: reads
return garbage-or-worse, *sometimes*, depending on whether the
allocator has re-let the boxes yet. The bug class is vicious precisely
because freed memory often still holds its old bytes for a while —
dangles tend to pass tests and detonate in production. The comment at
`mysh.c:180` is the author leaving a fence-post for *you*: this Pip is
a tenant, not an owner; here is its lease.

(Chapter 3 adds the heap's own demolition bugs — double-free,
use-after-free, leak — and the tool that catches all of them. For
today: lifetimes nest on the stack, and anything that doesn't nest goes
to the warehouse.)

---

## 1.11 Lab — verify everything with your own eyes

Theory claims; experiments collect. Run all four.

### Lab 1 — map the districts

Save as `probe.c` anywhere in the repo:

```c
#include <stdio.h>
#include <stdlib.h>

const char *g_literal     = "in .rodata";
int         g_initialized = 42;          /* .data */
int         g_zeroed;                    /* .bss  */

int main(void)
{
    int   local = 7;                     /* stack */
    char *heap  = malloc(16);            /* heap  */

    printf("text   (main)        %p\n", (void *)main);
    printf("rodata (literal)     %p\n", (void *)g_literal);
    printf("data   (init global) %p\n", (void *)&g_initialized);
    printf("bss    (zero global) %p\n", (void *)&g_zeroed);
    printf("heap   (malloc box)  %p\n", (void *)heap);
    printf("stack  (local)       %p\n", (void *)&local);

    free(heap);
    return 0;
}
```

```sh
gcc -Wall -o probe probe.c && ./probe && ./probe
```

Confirm: the six addresses ascend in the §1.1 map's order; the stack
address starts `0x7ff…`; and the two runs print *different* numbers
(ASLR) with *identical ordering*.

### Lab 2 — watch the districts of a live mysh

```sh
make            # builds the apps and shell
./shell/mysh
mysh> ls        # any command, to warm things up
# in ANOTHER terminal:
pgrep -f shell/mysh        # note the PID
cat /proc/<PID>/maps | grep -E "heap|stack|mysh"
```

You'll see the real estate: the `mysh` binary's `r-xp` (text), `r--p`
(rodata), `rw-p` (data+bss) mappings, then `[heap]`, then — far above —
`[stack]`. The map from §1.1 is not a metaphor; it's a file in `/proc`.

### Lab 3 — climb the tower in gdb

```sh
gdb shell/mysh
(gdb) break cat_run
(gdb) run
mysh> cat Makefile
# breakpoint hits:
(gdb) backtrace        # the tower: cat_run ← run_inproc ← run_pipeline ← main
(gdb) print tbl        # 7 blank-tag Pips, garbage — builder hasn't run yet
(gdb) next             # step over build_cat_argtable
(gdb) print tbl[0]     # now a real heap address (an arg_lit's box)
(gdb) print *help      # walk the string: the struct the builder tied for you
(gdb) print &tbl       # 0x7ff…  — cat_run's tray, stack district
(gdb) print &cmd_cat_spec   # 0x5…  — .data district, as promised
(gdb) continue
mysh> exit
```

`backtrace` *is* the tower of trays, printed bottom-up. `print &tbl`
versus `print tbl[0]` shows, in one breath, a stack box holding a heap
address — §1.5(b) in living color.

### Lab 4 — the decay trap, measured

Temporarily add to `linedit_read` (then revert):

```c
    fprintf(stderr, "[lab] sizeof buf = %zu\n", sizeof buf);
```

and to `main` just before the `linedit_read` call:

```c
    fprintf(stderr, "[lab] sizeof line = %zu\n", sizeof line);
```

Rebuild, run: `4096` upstairs, `8` downstairs. The size did not survive
the call; `bufsz` carried it. Revert the edits.

---

## 1.12 Chapter 1 — the rules to keep

> **R1.** A pointer is an 8-byte box holding an address. `&` reads a
> box's address; `*` walks to it. Declaration mirrors use.
>
> **R2.** Know your district: text/rodata/data/bss live forever; a tray
> (stack frame) lives one call; a warehouse box (heap) lives until
> `free`. Every pointer bug is a district/lifetime confusion.
>
> **R3.** To let a callee change a thing, pass the thing's address.
> When the thing is itself a pointer, that's a `**` — `&help`, `&line`,
> `&res`. (The out-parameter pattern: all 15 of your apps run on it.)
>
> **R4.** Arrays decay to pointers and *forget their size* — so the
> size rides shotgun (`linedit_read(…, line, sizeof line)`), or a
> sentinel terminates the data (`'\0'`; `argv[argc] == NULL`).
>
> **R5.** Pointer arithmetic strides by element size: `buf + pos` moves
> `pos` bytes; `lines + total` moves `total × 8`. `p[i] ≡ *(p + i)`.
>
> **R6.** Generic C APIs erase types into `void *` and make *you* the
> type system: re-label before walking (`*(const char * const *)a`),
> and thread state through the context slot (`qsort_r(..., &ctx)`).
>
> **R7.** Lifetimes: down the tower — always safe; up — never; sideways
> past the tray — heap only (`malloc(sizeof *a)` for every thread
> packet). When you borrow, know the lease
> (`/* points into tok_t; valid while pipeline runs */`).

---

## ⏭ Next: CHAPTER 2 — *Process Isolation, Mirror Machines, and the stdin Contamination Fix*

Pip hands the chalk to the **Forking Mirror**. One C function that
returns *twice*; clones that change costume into `/bin/date`; the three
Mirror Machines hidden in `mysh.c`; why `cd` can never, even in
principle, be delegated to a clone — and the forensic story of how the
shell's own script nearly became cat-food, until the command stream was
quarantined on fd 3 behind `g_cmd_fd`.

---
---

# CHAPTER 2
# Process Isolation, Mirror Machines, and the stdin Contamination Fix

> *In which the Forking Mirror flashes three times, a clone changes
> costume into `/bin/date`, `cd` proves that some work can never leave
> the room — and the shell's own script nearly becomes `cat`-food,
> until fd 0 is quarantined behind `g_cmd_fd`.*

---

## 2.1 One flash, two rooms — what `fork()` actually does

Chapter 1 mapped a single room: one virtual address space, six
districts, one tower of trays. The Forking Mirror is the device that
duplicates the *entire room* in one flash. In C, the flash is one line —
here is your shell aiming the Mirror before launching an external
command:

```c
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }

        if (pid == 0) {
```
*(`shell/mysh.c:236-239`)*

`fork()` is the strangest function you will ever call: **one call goes
in, two calls come out** — in two different processes. At the instant of
the flash, the kernel duplicates everything Chapter 1 taught you to see:
the text/rodata/data/bss districts, the heap (every tok word, every
`FILE` object *including its buffer* — remember that detail, it is the
villain of §2.5), and the whole tower of trays, frozen mid-call. Then
*both* rooms resume at the very next instruction:

```
              BEFORE the flash                          AFTER the flash

         ┌──────────────────────┐          ┌─────────────────┐  ┌─────────────────┐
         │   mysh    PID 41200  │          │ mysh  PID 41200 │  │ mysh  PID 48213 │
         │   line[]  stages[]   │  fork()  │ (the original)  │  │ (the clone, in  │
         │   heap: tok words,   │ ───────▶ │                 │  │  an isolated    │
         │         FILE buffers │          │ pid == 48213    │  │  dimension)     │
         │   fd cards 0,1,2,3   │          │ "I am parent    │  │ pid == 0        │
         └──────────────────────┘          │  of 48213"      │  │ "I am the copy" │
                                           └─────────────────┘  └─────────────────┘
                                                  both resume at the SAME line:
                                                  if (pid == 0) { ... }
```

The two rooms are byte-for-byte identical except for **one value**: in
the original, `fork()` returned the clone's process ID; in the clone, it
returned `0`. So `if (pid == 0)` is not an error check — it is the
question *"which side of the Mirror am I standing on?"*, and it is the
exact line where one program becomes two diverging realities.

What the clone receives, precisely:

| Property | Original | Clone |
|---|---|---|
| `fork()` return value | child's PID (e.g. 48213) | `0` |
| PID | unchanged | brand-new |
| address space (all six districts) | its own | a copy-on-write duplicate |
| file-descriptor table | its own card-box | a *photocopy of the card-box* — cards point at the **same** kernel bookmarks (§2.5) |
| signal dispositions, cwd, environment | unchanged | copied |

Two refinements before we move on:

**The Mirror is lazy (copy-on-write).** The kernel does not physically
copy a 20 MB room in the flash. It copies only the *floor plan* (the
page tables), marks every 4 KiB page shared-and-read-only, and copies a
page the first time either room writes to it. Forking is cheap precisely
because most clones immediately `exec` (next section) and never touch
most pages. The illusion of a full copy is perfect; the cost is deferred
and mostly never paid.

**Forks form a tree.** Every process has a parent; `fork()` adds a
child node. While `mysh` runs `date`, the OS sees:

```
   your-terminal ──▶ mysh (41200) ──▶ date (48213)
```

`pstree -p $$` will show you the live tree, your shell included.

### Your shell owns three Mirror Machines

`mysh` flashes the Mirror in exactly three places, and they have three
different purposes — keep this table; the whole chapter hangs on it:

| Mirror Machine | Where | The clone's fate |
|---|---|---|
| 1. external single command | `shell/mysh.c:236` (`run_inproc`) | costume change: `execvp("/bin/date")` |
| 2. external pipeline stage | `shell/mysh.c:413` (`run_pipeline`) | plumbing (`dup2`, Chapter 8) then `execvp` |
| 3. background job (`&`) | `shell/mysh.c:753` (main loop) | **no costume change** — the clone runs `run_pipeline()` *itself*, shell code in a parallel dimension, then `exit(rc)` |

Machine 3 is the odd one out — a clone that stays dressed as `mysh` and
executes your shell's own functions in its isolated room. It is also
the machine that makes the stdin contamination story (§2.5) deadly
serious.

---

## 2.2 The costume change — `execvp()`

A clone that stayed a copy of `mysh` forever would be useless for
running `/bin/date`. The second half of process creation is `exec` —
the **costume change**. Here is the complete child-side ritual from your
shell, all eight lines of it:

```c
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGPIPE, SIG_DFL);
            if (g_cmd_fd >= 0) close(g_cmd_fd);
            if (apply_redirs(s) < 0) _exit(1);
            execvp(s->argv[0], s->argv);
            fprintf(stderr, "mysh: %s: %s\n", s->argv[0], strerror(errno));
            _exit(127);
        }
```
*(`shell/mysh.c:239-247`)*

Four preparation lines, one transformation, and a two-line epitaph.

**What `execvp` does:** it demolishes the clone's entire room interior
— text, rodata, data, bss, heap, the whole tower of trays — and rebuilds
it from a different binary's blueprint. The clone walks into the Mirror
dimension dressed as `mysh` and walks out as `date`. (`execvp`: the
**v** means "argv comes as a vector — Chapter 1's NULL-fenced shelf";
the **p** means "search `$PATH` for the binary".) Crucially, some things
are bolted to the *room*, not the furniture, and survive:

| Demolished & rebuilt | Survives the costume change |
|---|---|
| `.text` `.rodata` `.data` `.bss` | the PID — same room number |
| the heap — *including the copied stdio buffers* | the fd card-box (minus close-on-exec cards, Chapter 8) |
| the stack tower | current working directory, environment |
| installed signal *handlers* (their code was just demolished → reset to default) | dispositions set to `SIG_IGN` — **ignored stays ignored** |

That last row explains preparation lines 1–2. At startup the shell
arranged to shrug off Ctrl-C:

```c
    /* SIGINT: shell ignores it; each child restores SIG_DFL before exec. */
    signal(SIGINT,  SIG_IGN);
    /* SIGPIPE: ignore broken pipes in the shell process itself. */
    signal(SIGPIPE, SIG_IGN);
```
*(`shell/mysh.c:552-555`)*

The clone inherits `SIG_IGN`, and `SIG_IGN` *survives exec*. Without
`signal(SIGINT, SIG_DFL)` before the costume change, `sleep 100` would
be born deaf to Ctrl-C — you could never interrupt it. Eight lines, and
two of them exist purely because of a subtle exec-survival rule. (Line
3, `close(g_cmd_fd)`, is §2.5's punchline; line 4 wires `<`/`>`
redirections — Chapter 8.)

**Why fork *then* exec, instead of one "spawn" call?** Because the gap
between flash and costume is where the child configures *itself*, in
plain C, using the parent's own logic — reset signals, snip private
fds, rewire stdin/stdout — with zero effect on the parent. Windows'
`CreateProcess` takes a dozen parameters to describe what the child
should look like; Unix says: *become* the child, fix the room yourself,
then put on the costume. Your eight-line block is that philosophy,
verbatim.

**The epitaph.** On success, `execvp` **never returns** — the code that
called it no longer exists in that room. If you see the line after it
execute, the costume change failed (command not found, not executable),
so the clone prints the reason and dies with the shell convention for
"command not found":

```c
            execvp(s->argv[0], s->argv);
            fprintf(stderr, "mysh: %s: %s\n", s->argv[0], strerror(errno));
            _exit(127);
```

Why `_exit` and not `exit`? `exit()` runs `atexit` handlers and flushes
stdio — *the parent's inherited habits*. Your linedit module registers
`atexit(raw_disable)` (`shell/linedit.c:87`); a half-built clone must
not run handlers like that, nor re-flush buffered output it inherited a
copy of. `_exit()` leaves the room without touching anything. Notice the
flushes on the *other* side of the flash, in the parent, just before
forking:

```c
        /* External command: fork so the child can exec without disturbing us. */
        fflush(stdout);
        fflush(stderr);

        pid_t pid = fork();
```
*(`shell/mysh.c:232-236`)*

Empty the outbox **before** duplicating the room, or both rooms
eventually mail the same letters — every buffered byte would print
twice. (Hold this thought: it is the *output-side* twin of the
input-side disease in §2.5.) And contrast Mirror Machine 3, where the
background clone finishes its pipeline and calls plain `exit(rc)`
(`shell/mysh.c:761`) **deliberately** — that clone ran real work in its
room and its own buffered output *must* flush. `_exit` for aborted
costume changes, `exit` for a clone that did honest work: your shell
uses each in exactly the right place.

---

## 2.3 Collecting the ghost — `waitpid()` and the `$?` circuit

When a clone dies, it does not fully vanish. The kernel keeps a small
stub — PID and exit status — until the parent collects it. Uncollected,
the stub lingers as a **zombie** (literally flagged `Z` in `ps`): the
clone's ghost, haunting the process table, holding one slot forever.
Collecting it is called *reaping*:

```c
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
```
*(`shell/mysh.c:249-251`)*

`waitpid` blocks until the child exits (this is *why* your shell pauses
until `sleep 5` finishes), then fills `status` — note Chapter 1's
out-parameter pattern, `&status` — with a bit-packed integer that you
never poke directly. The macros decode it: `WIFEXITED` asks "did it
exit normally, rather than die by signal?"; `WEXITSTATUS` extracts the
code it passed to `_exit`. Your `? : 1` maps signal-deaths to a plain
failure code.

Follow one byte of truth across two dimensions — the full `$?` circuit:

```
  clone: _exit(127)                              (mysh.c:246)
     └▶ kernel stub (zombie holds the 127)
         └▶ parent: waitpid(&status) → WEXITSTATUS → 127   (mysh.c:249-251)
             └▶ last_status = run_pipeline(...)            (mysh.c:767)
                 ├▶ tok_split expands "$?" → "127"         (tok.h:18)
                 └▶ the prompt itself confesses:           (mysh.c:601-603)
```
```c
            char prompt[32];
            if (last_status != 0)
                snprintf(prompt, sizeof prompt, "mysh [%d]> ", last_status);
```

One more reaper, easy to miss, at the top of the prompt loop:

```c
        /* Reap any finished background jobs to avoid zombies. */
        while (waitpid(-1, NULL, WNOHANG) > 0);
```
*(`shell/mysh.c:594-595`)*

`-1` means "any child of mine"; `WNOHANG` means "don't block — return 0
if nobody's dead yet"; the `while` drains *all* waiting ghosts in one
sweep. Background jobs (Mirror Machine 3) exit whenever they please;
nobody is `waitpid`-ing on them at that moment, so they zombify — and
this line, once per prompt, lays them to rest. Without it, a long
session of `cmd &` would leak one ghost per job. (Lab 4 lets you watch
a zombie appear and get reaped.)

---

## 2.4 The room that never flashes — `run_inproc` and the in-process bet

Now the contrast the Mirror exists to illuminate. When the command is
*internal* — found in the registry — your shell does something most
shells don't: **nothing exotic at all.**

```c
static int run_inproc(stage_t *s)
{
    cmd_spec_t *spec = reg_find(s->argv[0]);

    if (spec) {
        /* Internal command: open any redirected files and pass explicit streams. */
        FILE *in  = stdin;
        FILE *out = stdout;
```
*(`shell/mysh.c:197-204`)*
```c
        int rc = spec->run(s->argc, s->argv, in, out);
```
*(`shell/mysh.c:225`)*

No flash. No costume. `spec->run(...)` is an ordinary function call
through a function-pointer Pip (Chapter 4): a new tray on the *same*
tower, in the *same* room, costing nanoseconds. The Mirror path —
syscall, page-table copying, ELF loading, dynamic linking — costs
somewhere between a hundred microseconds and a few milliseconds:
**thousands of times more**. For one `cat`, invisible. For a 10,000-line
script of built-ins, the difference between instant and sluggish.

But the bet has two sides, and you should know exactly what your shell
wagered:

| | in-process (`spec->run`) | Mirror (`fork` + `execvp`) |
|---|---|---|
| cost | a function call | flash + costume change |
| a crash in the command | **kills the whole shell** | kills only the clone |
| a memory leak | accumulates in the shell forever | erased with the clone's room |
| `chdir`, `setenv` | affect the shell — *sometimes that's the point* | lost when the clone dies |
| stdin/stdout | must be passed explicitly as `FILE *` parameters | inherited as fd 0/1, free |

Look at the third row twice, because it contains a *logical necessity*,
not a preference. Consider `cd`:

```c
                if (chdir(dir) < 0) {
                    fprintf(stderr, "mysh: cd: %s: %s\n", dir, strerror(errno));
                    last_status = 1;
                } else {
                    if (prev[0]) setenv("OLDPWD", prev, 1);
                    last_status = 0;
                }
```
*(`shell/mysh.c:730-736`)*

Imagine delegating this to the Mirror: the clone wakes in its parallel
dimension, walks to `/tmp` *in that dimension*, and dies. The original
room **has not moved** — process isolation, the Mirror's whole virtue,
guarantees it. `cd` run via fork would be a no-op *by the laws of the
universe it lives in*. This is why every shell ever written has
built-ins: not for speed, but because some commands must mutate *this*
room. (Same for `exit`, and for `VAR=value` — `shell/mysh.c:689-699`.)

The last row of the table explains a design decision you made back in
`cmd_spec.h` without perhaps noticing its weight:

```c
    int  (*run)(int argc, char **argv, FILE *in_stream, FILE *out_stream);
```
*(`include/cmd_spec.h:10`)*

An exec'd program gets stdin/stdout for free, as inherited fd 0/1. An
in-process command **cannot** — it shares the room's real stdin/stdout
with the shell itself. So the streams travel as explicit parameters —
Chapter 1's "pass the table by reference" applied to I/O. That one
signature is what makes your commands *relocatable*: the same
`cat_run`, unmodified, runs as a standalone binary, as an in-process
built-in, as a pipeline thread (Chapter 5) — and as a *library call*.
Watch the shell's `@` mode invoke `fetch` like a function:

```c
    char *fargv[] = {
        "fetch", "-H", AI_HOST, "-p", AI_PORT, "--",
        (char *)prompt, NULL
    };
    int fargc = (int)(sizeof fargv / sizeof fargv[0]) - 1;
    return cmd_fetch_spec.run(fargc, fargv, in, out);
```
*(`shell/mysh.c:521-526`)*

A synthetic NULL-fenced argv built on the tray (§1.3's shelf,
hand-made), and a direct call — the comment above it says it best: *"no
fork, no second parse"* (`shell/mysh.c:512-513`). The Mirror is
powerful; **not needing it is a superpower.**

---

## 2.5 The stdin Contamination Fix — `g_cmd_fd`

Everything in this chapter converges here: the Mirror's perfect copying
(§2.1), clone inheritance (§2.2), Mirror Machine 3, and one humble
`dup()` that keeps your shell's scripts from eating themselves.

### Where commands come from

```c
    /* Script mode: mysh script.sh */
    FILE *input = stdin;
    if (argc >= 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            fprintf(stderr, "mysh: %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        /*
         * stdin is a pipe or redirect.  Dup it to a fresh fd and read
         * commands from there, leaving fd 0 untouched.  Forked pipeline
         * children then inherit a clean fd 0 with no buffered command text.
         */
        g_cmd_fd = dup(STDIN_FILENO);
        if (g_cmd_fd >= 0) {
            input = fdopen(g_cmd_fd, "r");
            if (!input) { close(g_cmd_fd); g_cmd_fd = -1; /* fall back */ }
        }
    }
```
*(`shell/mysh.c:557-576`)*

Three modes, three risk profiles:

1. **Script file** (`mysh script.sh`): `fopen` gives a private `FILE`
   on a fresh fd (3+). fd 0 still points at your terminal. Naturally
   safe.
2. **Interactive tty**: linedit reads keystrokes; there *is* no
   pre-existing text to fight over. Safe.
3. **Piped/redirected stdin** (`printf 'ls\ndate\n' | mysh`): the
   commands arrive *on fd 0 itself* — the very stream every child and
   every stdin-reading command also drinks from. **This is the danger
   mode**, and the `else if` branch is the fix. To understand it, you
   need one piece of kernel furniture Chapter 1 didn't cover.

### Cards and bookmarks — a 90-second kernel primer

A file descriptor is **not** a file. It is an index **card** in your
room's card-box (the per-process fd table), and the card points at a
kernel-side **bookmark** (the *open file description*) which holds the
actual reading offset; the bookmark, in turn, references the real pipe
or file:

```
     mysh's card-box (fd table)              kernel bookmarks
     ┌───────────────────────┐
     │ 0 ●───────────────────┼─────┐     ┌──────────────────────────┐
     │ 1 ●─▶ (tty, output)   │     ├───▶ │  the command pipe        │
     │ 2 ●─▶ (tty, errors)   │     │     │  ONE reading offset      │
     │ 3 ●───────────────────┼─────┘     └──────────────────────────┘
     └───────────────────────┘
       after dup(): TWO cards, ONE bookmark.
       dup() copies cards — never bookmarks.
```

Two laws follow, and both bite in this section:

- **`dup()` duplicates a card, not a bookmark.** Cards 0 and 3 above
  share one reading offset.
- **`fork()` photocopies the whole card-box** into the clone's room —
  and the copies *still point at the same bookmarks*. Reading progress
  is shared **across dimensions**.

One more piece: a `FILE *` (stdio stream) is a heap object — call it
the room's **furniture** — that wraps a card and adds a buffer. When
you `fgets` one line, glibc doesn't read one line; it `read()`s a full
block (kilobytes) through the card, advancing the *shared bookmark*
that far, and parks everything in the furniture. And because furniture
is heap, **the Mirror copies it, buffer contents and all.**

### The disease — run the thought experiment

Mentally delete the `dup` branch, so `input == stdin`: the command loop
and the rest of the world share one `FILE` and one card. Now run:

```sh
printf 'cat &\nls\n' | mysh        # hypothetical mysh WITHOUT the fix
```

1. The loop calls `fgets`. glibc slurps **the entire script** —
   `"cat &\nls\n"` — through fd 0 into `stdin`'s furniture. The kernel
   bookmark is now at end-of-pipe. The command `ls` *no longer exists
   in the kernel*; it exists only as bytes in one room's furniture.
2. The shell processes `cat &` → **Mirror Machine 3** flashes. The
   clone's room contains a perfect copy of `stdin`'s furniture —
   *including the unprocessed `"ls\n"`*. The command stream now exists
   **twice, in two dimensions.**
3. In the clone, `run_inproc` hands internal `cat` its input stream:
   `FILE *in = stdin;` (`mysh.c:203`). `cat` drains the furniture —
   and prints **`ls`** as *data*. A mirror clone just ate (and leaked)
   your command stream.
4. Meanwhile the original room *also* still holds `"ls\n"`, so the
   shell *also* executes `ls`. One line of script: printed once as
   `cat` output, executed once as a command. Two realities, both wrong.

And note the quieter sibling: no fork is even needed. A plain
foreground internal `cat` (no `&`) would *also* read the shared
furniture in step 3 — same room, same theft. Any stdin-reading
internal command becomes a command-stream eater the moment the command
loop and the commands share one `FILE`.

### The fix — one `dup`, one `fdopen`

```c
        g_cmd_fd = dup(STDIN_FILENO);
        if (g_cmd_fd >= 0) {
            input = fdopen(g_cmd_fd, "r");
```
*(`shell/mysh.c:571-573`)*

Mint a second card (fd 3) for the same pipe, wrap it in **its own
private furniture** (`input`), and let the command loop read only from
that. The contamination chain breaks at both ends:

- The slurped, unprocessed command text now lives in `input`'s
  furniture — which **no command ever reads**. Internal commands get
  `stdin`, whose furniture stays empty; clones copy `input`'s furniture
  but never *read* it (Machine 3 clones run `run_pipeline`, not the
  command loop).
- fd 0 remains a clean card on the pipe, so children inherit exactly
  what Unix promises them — a real stdin, "unmolested", as the comment
  at `shell/mysh.c:65` puts it.

(Also admire the failure path: if `dup` or `fdopen` fails, it closes
what it opened, resets `g_cmd_fd = -1`, and *falls back* to the old
shared-stdin behavior — degraded, never broken. `shell/mysh.c:574`.)

### The other half: every clone snips the spare key

The fix mints a second card to the command stream — which means every
clone's photocopied card-box *contains that card too*. `dup()`'d cards
are **not** close-on-exec by default, so an exec'd `/bin/date` would
wake up holding fd 3 = your script. Any program that reads its spare
fds — or any buggy `read(3, ...)` — could *still* eat commands. So all
three Mirror Machines snip the card first thing after the flash:

```c
            if (g_cmd_fd >= 0) close(g_cmd_fd);
```
*(Machine 1: `shell/mysh.c:242` — Machine 2: `mysh.c:424` — Machine 3: `mysh.c:759`)*

— exactly as the declaration promised it would be:

```c
static int g_cmd_fd = -1;   /* set in main(), closed in every fork child */
```
*(`shell/mysh.c:68`)*

After the snip, precisely **one room in the universe** holds a card to
the command stream's private lane: the original shell. (Your pipeline
code achieves the same hygiene for its pipes declaratively, with
`fcntl(pfd[0], F_SETFD, FD_CLOEXEC)` at `mysh.c:327-328` — "this card
self-destructs on costume change." Two idioms, one principle; Chapter 8
compares them properly.)

### The conclusion, in one picture

```
 WITHOUT THE FIX — one card, shared furniture, clones eat commands
 ══════════════════════════════════════════════════════════════════
   printf 'cat &\nls\n' ══ pipe ══▶ ● bookmark (at EOF — all slurped)
                                      ▲
      ROOM 1: mysh ───── fd 0 card ───┘
      ┌───────────────────────────────────────────────┐
      │ FILE *stdin furniture:  │ l s \n │  ◀── the    │
      │   command loop reads here ──┐     unread      │
      │   internal cat reads here ──┤     script!     │
      │                             ▼                 │
      │      SAME BOX — whoever reads first, wins     │
      └───────────────────────────────────────────────┘
                       │  "cat &" → the Mirror flashes (Machine 3)
                       ▼
      ROOM 2 (clone): furniture COPIED — "ls\n" exists TWICE
      ┌───────────────────────────────────────────────┐
      │ its cat drains the copy → prints "ls" as DATA │
      └───────────────────────────────────────────────┘
      ...while ROOM 1 still holds "ls\n" → ls ALSO runs.
      One command: printed once, executed once — intended never.

 WITH THE FIX — two cards, quarantined furniture, zero eaters
 ══════════════════════════════════════════════════════════════════
   printf 'cat &\nls\n' ══ pipe ══▶ ● bookmark
                                      ▲    ▲
      ROOM 1: mysh ── fd 0 card ──────┘    │
                   └─ fd 3 card ───────────┘  = g_cmd_fd   (dup, mysh.c:571)
      ┌───────────────────────────────────────────────┐
      │ FILE *input (fd 3): │ l s \n │ ◀ commands —   │
      │   read ONLY by the command loop    PRIVATE    │
      │ FILE *stdin (fd 0): │ (empty) │ ◀ what every  │
      │   worker and clone sees            CLEAN      │
      └───────────────────────────────────────────────┘
                       │  "cat &" → the Mirror flashes
                       ▼
      ROOM 2 (clone): first act — close(g_cmd_fd) ✂ fd 3 card
      ┌───────────────────────────────────────────────┐
      │ its cat reads fd 0 → bookmark at EOF → EOF;   │
      │ the copied `input` furniture sits inert —     │
      │ nothing in this room ever reads it            │
      └───────────────────────────────────────────────┘
      ROOM 1 alone reads "ls\n" → ls runs exactly once.   ∎
```

That is the whole fix: **never let workers and the control plane share
a buffered stream.** Give the control plane its own card and its own
furniture; shred the spare key at every flash. The same disease — a
control channel slurped into one process's userspace buffer and then
duplicated by fork — recurs in web servers, cron daemons, and build
systems; you have now debugged it once forever.

### Sidebar: the contract your shell chose

Full honesty: POSIX shells handle danger-mode differently. `bash`
*wants* a script to be able to feed its own text to a command's stdin
(the `read` builtin depends on it), so on a pipe bash reads cautiously
— tiny reads, never past the current line — leaving the remainder in
the kernel for whoever reads next. mysh instead **quarantines** the
command stream: commands never become data, scripts behave identically
piped or from a file; the price is that a mysh script can't feed text
to `read`-style consumers. Both contracts are defensible; what is *not*
defensible is the broken middle — buffered slurping on a shared `FILE`
— and the `g_cmd_fd` fix is precisely what moves mysh from the broken
middle to a sound contract. Lab 3 makes the two contracts visible side
by side.

---

## 2.6 Lab — watch the Mirror with your own eyes

### Lab 1 — strace the flash and the costume change

```sh
printf 'date\nexit\n' | strace -f -e trace=clone,execve,wait4 ./shell/mysh 2>&1 | grep -E 'clone|execve|wait4'
```

You'll see (PIDs vary): the shell's own costume change
(`execve("./shell/mysh", ...)`), then for `date`: `clone(...) = <pid>`
— the flash (glibc implements `fork()` via the `clone` syscall) —
then, tagged with the child's PID, `execve("/usr/bin/date", ["date"],
...)`, then the parent's `wait4(<pid>, ...)`. Three syscalls; map them
to `mysh.c:236`, `:244`, `:250`.

### Lab 2 — photograph the two cards, one bookmark

```sh
{ printf 'date\n'; sleep 30; } | ./shell/mysh &
sleep 1
ls -l /proc/$(pgrep -x mysh | head -1)/fd
```

Expect: `0 -> pipe:[N]` and `3 -> pipe:[N]` — **the same inode N**.
That is `g_cmd_fd`, photographed: two cards, one bookmark. (`kill %1`
to clean up.)

### Lab 3 — two shells, two contracts

```sh
printf 'cat\nls\n' | bash          # bash: prints the TEXT "ls" — the
                                   # script fed itself to cat
printf 'cat\nls\n' | ./shell/mysh  # mysh: cat prints nothing (clean,
                                   # drained stdin) — and ls RUNS
```

Same input, two defensible contracts (§2.5 sidebar). Before the fix,
mysh would have been in the broken middle: `cat` replaying buffered
command text while the loop lost it.

### Lab 4 — ghosts, reaping, and the `$?` circuit

```sh
./shell/mysh
mysh> false
mysh [1]> echo $?            # the circuit: WEXITSTATUS → last_status → $?
1
mysh> nosuchcmd              # exec fails in the clone → _exit(127)
mysh: nosuchcmd: No such file or directory
mysh [127]> sleep 5 &
[bg] 48213
```

Wait six seconds **without pressing Enter**, then from another
terminal: `ps -o pid,stat,comm -p 48213` → `Z ... mysh <defunct>` — a
ghost, waiting. Now press Enter in mysh (the prompt-loop reaper at
`mysh.c:595` runs) and `ps` again: gone. You have witnessed a zombie's
entire afterlife.

---

## 2.7 Chapter 2 — the rules to keep

> **R8.** `fork()` = one flash, two rooms: the whole address space,
> card-box, and stdio furniture duplicated (lazily, copy-on-write).
> It returns **twice**; `if (pid == 0)` asks "which side of the Mirror
> am I on?".
>
> **R9.** The gap between fork and exec is the point: the clone
> configures *itself* — signals to `SIG_DFL`, private fds closed,
> redirections wired — using plain C, before the costume change.
>
> **R10.** `exec` demolishes the room's interior but keeps PID, fd
> cards, cwd, and *ignored* signals (`SIG_IGN` survives; handlers
> don't). Failure path: `stderr` + `_exit(127)`. Never `exit()` in a
> half-built clone — `_exit` skips inherited atexit handlers and
> double-flushes; `fflush` *before* the flash for the same reason.
>
> **R11.** Every clone must be reaped (`waitpid`) or it haunts the
> process table as a zombie. For background jobs: `waitpid(-1, NULL,
> WNOHANG)` in a loop, once per prompt.
>
> **R12.** Work that must mutate *this* room — `cd`, `exit`,
> `VAR=value` — cannot be delegated to a clone, by the very isolation
> that makes clones safe. Built-ins exist by necessity, not preference.
>
> **R13.** In-process execution is thousands of times cheaper and
> composes like a library (`cmd_fetch_spec.run(...)`) — paid for in
> isolation: a crash or leak belongs to the shell. The `FILE *in/out`
> parameters in `cmd_spec_t` are what make commands relocatable across
> all of these worlds.
>
> **R14.** Cards (fds) are per-room; bookmarks (open file
> descriptions) are shared — by `dup()` *and* by `fork()`. Therefore:
> flush output before flashing, quarantine the command stream on its
> own card + furniture (`g_cmd_fd`), and snip the spare key in every
> clone (`close(g_cmd_fd)` × 3).

---

## ⏭ Coming up in CHAPTER 3 — *The Heap: Pip's Warehouse District, Dynamic Buffers, and Valgrind Safeguards*

Chapters 1–2 used the warehouse from the outside; Chapter 3 moves in.
Pip gets a ring of keys and a ledger, and we study the **ownership
discipline** your code already practices — and one place where two of
your own files *disagree about how to call `realloc` safely* (one of
them is right; we'll find out which):

- the `malloc` / `realloc` / `free` contracts, and what the allocator
  actually does with the warehouse;
- **growable buffers**, your codebase's favorite move: `cat_json`'s
  doubling loop (`cmd_cat.c:95-103`), `sort`'s `read_all_lines`
  (`cmd_sort.c:117-140`), and `fetch`'s reply accumulator
  (`cmd_fetch.c:368-380`) — three implementations of one idea, compared
  line by line, plus the amortized-cost math of why doubling is O(1)
  per byte;
- ownership rules: who frees, exactly once, on every path — including
  error paths (`free(batch)`, `free(reply)`, `arg_freetable`);
- the rogues' gallery: leak, double-free, use-after-free — and hunting
  all three in rahulbox with **valgrind**.

---
---

# CHAPTER 3
# The Heap: Pip's Warehouse District, Dynamic Buffers, and Valgrind Safeguards

> *In which Pip rents storage boxes from the Warehouse Clerk, three of
> your buffers race to grow a megabyte, the `realloc` trap finally
> claims its promised victim — and an auditor named valgrind reads the
> ledger out loud.*

---

## 3.1 The Warehouse District — renting boxes from the Clerk

Chapter 1 placed the heap on the map; Chapter 2 showed the Mirror
copying it wholesale. Now we walk inside.

The warehouse is run by **the Clerk** — glibc's allocator. The Clerk
rents out storage boxes of any size you ask for, and his entire
business runs on three counter services:

| Call | Pip's transaction with the Clerk |
|---|---|
| `p = malloc(n)` | *"Rent me a box of at least `n` bytes."* The Clerk finds (or carves) a free box, writes a **ledger entry just before it** — the box's size and bookkeeping flags — and hands Pip the key (the address of byte 0). The box's contents are **uninitialized**: whatever the last tenant left behind. |
| `free(p)` | *"I'm returning this key."* The Clerk reads the ledger entry sitting just before the address to learn the size — this is why `free` never takes a length, and why the key must be **exactly** the one he issued. The box returns to the free list. **Its contents are not erased.** |
| `q = realloc(p, n)` | *"I need this box bigger."* If the neighboring space is free, the Clerk extends the box in place — same key back. Otherwise he rents a *new* box, **moves your contents** (`memcpy`), retires the old one, and hands back a new key. And the fine print that this whole chapter turns on: **if the warehouse can't satisfy you, he returns `NULL` — and your old box stays rented, intact, under the old key.** |

```
        THE WAREHOUSE (heap district)

   ┌─────┬──────────────────┬─────┬─────────┬───────────────┐
   │ hdr │  box: 8192 B     │ hdr │ box:64B │   free space  │
   └─────┴──────────────────┴─────┴─────────┴───────────────┘
      ▲           ▲
      │           └─ reply ●━━ Pip's key points at byte 0
      └─ the Clerk's ledger entry lives immediately BEFORE
         the box — which is why writing to reply[-1] corrupts
         the ledger ("heap corruption"), and why free(reply)
         needs no size argument
```

Three pieces of fine print your code already relies on:

- **`realloc(NULL, n)` ≡ `malloc(n)`.** That's why `cat_json` and
  `fetch` can start with `char *buf = NULL;` / `char *reply = NULL;`
  and use *one* growth path for the first allocation and every later
  one — no special first-time case.
- **`free(NULL)` is a no-op.** A key tied to nothing can be "returned"
  harmlessly — the foundation of the free-then-NULL idiom in §3.5.
- **The Clerk's desk is thread-safe.** When Chapter 5's Threaddy
  Speedrunners all call `malloc` from their pipeline stages at once,
  glibc serializes (and partitions) access internally. Shared room,
  one ledger, no torn entries.

Why rent at all, when Chapter 1's trays are free? Two reasons, and
they bound this chapter: **lifetime** — a box outlives the tray that
rented it (`stage_arg_t`, R7) — and **size unknown until runtime**,
which is every buffer in this chapter: `cat --json` cannot know the
file size, `sort` cannot know the line count, `fetch` cannot know the
reply length. When you can't size it at compile time, you rent.

---

## 3.2 The trio rides again — one invariant, three disguises

Chapter 1 (§1.6) met the *pointer / capacity / length* trio in
`linedit`'s fixed buffer. Every growable buffer in your codebase is
the same trio, now with the power to re-rent:

| | the Pip (key) | capacity (box size) | length (bytes used) | element type |
|---|---|---|---|---|
| `cat_json` (`cmd_cat.c:89-91`) | `buf` | `cap` | `size` | bytes |
| `read_all_lines` (`cmd_sort.c:119-121`) | `lines` | `cap` | `count` | `char *` — a shelf of Pips! |
| `fetch` recv loop (`cmd_fetch.c:344-346`) | `reply` | `cap` | `total` | bytes |

The invariant all three maintain: `0 ≤ length ≤ capacity`; bytes
`[0, length)` are meaningful; bytes `[length, capacity)` are rented
but **uninitialized** (valgrind literally tracks that distinction,
§3.6). Growth is triggered by exactly one condition — *the incoming
data won't fit* — and handled by exactly one move: re-rent bigger,
never smaller, never by a constant.

---

## 3.3 The Amortized Growth Pattern — three implementations, line by line

Here are the three growth engines, side by side. First `cat_json`,
filling from a file:

```c
    while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (size + n + 1 > cap) {
            cap = (size + n + 1) * 2 + 4096;
            buf = realloc(buf, cap);
            if (!buf) { fprintf(stderr, "cat: out of memory\n"); return 1; }
        }
        memcpy(buf + size, tmp, n);
        size += n;
    }
```
*(`apps/cat/cmd_cat.c:95-103`)*

Then `read_all_lines`, growing a shelf of line-Pips:

```c
    int    cap   = 64;
    int    count = 0;
    char **lines = malloc(cap * sizeof(char *));
    if (!lines) return -1;
```
*(`apps/sort/cmd_sort.c:119-122`)*
```c
    while ((len = getline(&buf, &bufsz, fp)) != -1) {
        if (count == cap) {
            cap *= 2;
            char **tmp = realloc(lines, cap * sizeof(char *));
            if (!tmp) { free(buf); free(lines); return -1; }
            lines = tmp;
        }
        lines[count++] = strndup(buf, len);
    }
```
*(`apps/sort/cmd_sort.c:128-136`)*

Then `fetch`, accumulating a network reply:

```c
        if (total + (size_t)n > cap) {
            cap = cap ? cap * 2 : 8192;
            while (cap < total + (size_t)n) cap *= 2;
            char *grown = realloc(reply, cap);
            if (!grown) {
                FETCH_FAIL("recv", "out of memory");
                free(reply);
                close(fd);
                arg_freetable(tbl, 6);
                return 1;
            }
            reply = grown;
        }
        memcpy(reply + total, buf, (size_t)n);
        total += (size_t)n;
```
*(`apps/fetch/cmd_fetch.c:368-383`)*

Same skeleton, three times: *check fit → grow geometrically → copy in
→ advance length*. The growth lines differ in flavor, not in kind:

| | growth rule | flavor |
|---|---|---|
| `cat_json` | `cap = (size + n + 1) * 2 + 4096` | double **the need**, plus slack (the `+ 1` reserves NUL space it never actually uses — harmless headroom) |
| `read_all_lines` | `cap *= 2` from 64 slots | double **the capacity** |
| `fetch` | `cap = cap ? cap * 2 : 8192` then `while (cap < needed) cap *= 2` | double the capacity, with a catch-up loop — armor for a future where a single `recv` could exceed one doubling (today `n ≤ 4096 < 8192`, so it never fires) |

All three are **geometric**. That word is the entire performance story.

### The math: why doubling, not `+1` (or `+4096`)

Suppose you grow a buffer to N = 1 MiB. Every time the box moves, the
Clerk copies `length` bytes. Compare strategies by *total bytes ever
copied*:

**Grow by 1 byte:** appending byte k forces a copy of the k−1 bytes
already there. Total: 1 + 2 + ⋯ + (N−1) = N(N−1)/2 ≈ **5.5 × 10¹¹
copies** — half a *trillion* byte-moves and a million Clerk visits to
build one megabyte. That is O(N²): double the data, *quadruple* the
work.

**Grow by a fixed 4096:** better, still quadratic — 256 visits, but
each copy is bigger than the last: ≈ 1.3 × 10⁸ copies. Fixed
increments only divide the N² constant; they never remove it.

**Double:** copies happen only at capacities c, 2c, 4c, …, N/2, and
the total is a geometric series with a closed form worth memorizing:

```
        c + 2c + 4c + ⋯ + N/2  =  N − c  <  N
```

Building 1 MiB by doubling from 4 KiB: **9 Clerk visits, under one
megabyte of total copying** — ever. Summarized:

| strategy | Clerk visits (reallocs) | total bytes copied | scaling |
|---|---|---|---|
| `+1` byte | 1,048,576 | ≈ 550,000,000,000 | O(N²) |
| `+4096` fixed | 256 | ≈ 134,000,000 | O(N²), smaller constant |
| `× 2` | 9 | < 1,048,576 | **O(N)** |

The punchline is the phrase **amortized O(1) append**: since all
copies ever performed sum to less than N, the *average* byte is moved
less than one extra time, no matter how large N grows. A few
individual appends are expensive (the ones that trigger a move); their
cost is "amortized" — spread — across the thousands of cheap appends
between moves. Your three loops all bought this guarantee, each in its
own dialect.

(Two footnotes for honesty. First: the analysis is a worst case — the
Clerk often extends in place or, for mmap-backed big boxes, remaps
pages without copying at all. Second: the factor needn't be 2 — some
allocator-friendly libraries use 1.5 to make retired boxes reusable
sooner. Any factor > 1 is O(N); no constant increment is.)

---

## 3.4 The realloc Trap — and the pointer-proxy verdict

Chapter 1 promised that two of your files disagree about how to call
`realloc`, and that one of them is right. Time to pay up. Stare at the
two shapes:

```c
            buf = realloc(buf, cap);                       /* cat_json   */
            if (!buf) { fprintf(stderr, "cat: out of memory\n"); return 1; }
```
*(`apps/cat/cmd_cat.c:98-99`)*

```c
            char *grown = realloc(reply, cap);             /* fetch      */
            if (!grown) {
                /* ... */
                free(reply);
                /* ... */
                return 1;
            }
            reply = grown;
```
*(`apps/fetch/cmd_fetch.c:371-380`)*

**`cat_json` is the trap.** Re-read the Clerk's fine print from §3.1:
on failure, `realloc` returns `NULL` *and the old box stays rented,
intact, under the old key*. Now watch what `buf = realloc(buf, cap)`
does on that day:

```
   before:   buf ●━━━━━━▶ [hdr | 300 KB of file content]   (rented)

   the Clerk says no:  realloc returns NULL

   after:    buf ●━━━▶ NULL
                          [hdr | 300 KB of file content]   (STILL rented —
                                                            and now NO KEY
                                                            EXISTS, anywhere)
```

Pip untied his **only** string from the old box in order to catch the
Clerk's answer — and the answer was nothing. The 300 KB box is still
rented; no pointer in the universe holds its address; it can never be
freed. That is the definition of a **leak**, and no later code can fix
it — the subsequent `return 1` doesn't matter; even `free(buf)` would
just be `free(NULL)`. The architectural error is *overwriting your
only key before checking the answer*.

**`fetch` and `sort` use the proxy.** A second, temporary Pip —
`grown` in fetch, `tmp` in `read_all_lines` — catches the Clerk's
answer first:

```c
            char **tmp = realloc(lines, cap * sizeof(char *));
            if (!tmp) { free(buf); free(lines); return -1; }
            lines = tmp;
```
*(`apps/sort/cmd_sort.c:131-133`)*

On failure, `lines` still holds the **valid old key** — so the error
path can do the honest thing: `free(lines)` returns the box, nothing
leaks, and the function reports `-1`. Only after the NULL-check does
the real Pip re-tie (`lines = tmp;`). The cost of total safety: one
8-byte stack variable and one assignment.

> **The rule, suitable for tattooing:** never `p = realloc(p, n)`.
> Always `q = realloc(p, n); if (!q) { /* p is still good */ }
> p = q;`

So the verdict on the Chapter 1 teaser: **`sort` and `fetch` are
right; `cat_json` is wrong.** In fairness to `cat_json`, the bug fires
*only* when the warehouse is exhausted — which is rare, and which many
programs answer by dying anyway. But recall R13: your shell runs `cat`
**in-process**. A leak in `cat_json` doesn't die with a clone — it
accumulates in the shell, forever. In rahulbox, of all projects, the
proxy is not pedantry. (Exercise 3 in the lab applies the three-line
fix.)

---

## 3.5 The Rules of Ownership — who frees what, on every path

A box may have many strings tied to it, but at any moment exactly
**one Pip is the owner** — the one whose duty is to return the key.
C will not track this for you; you track it the way your code already
does: by *contract*, stated in comments, signatures, and man pages.

### Ownership transfer: the life of `batch`

`sort` moves whole shelves of ownership around, and never drops a key
on the happy path. Watch the macro that gathers lines from each input
file:

```c
    #define APPEND_LINES(fp) do { \
        char **batch = NULL; \
        int n = read_all_lines(fp, &batch); \
        if (n < 0) { \
            fprintf(stderr, "sort: out of memory\n"); \
            arg_freetable(tbl, 9); return 1; \
        } \
        if (total + n > cap) { \
            cap = (total + n) * 2 + 64; \
            char **tmp = realloc(lines, cap * sizeof(char *)); \
            if (!tmp) { \
                fprintf(stderr, "sort: out of memory\n"); \
                free(batch); arg_freetable(tbl, 9); return 1; \
            } \
            lines = tmp; \
        } \
        memcpy(lines + total, batch, n * sizeof(char *)); \
        total += n; free(batch); \
    } while (0)
```
*(`apps/sort/cmd_sort.c:213-231`)*

Trace the deed of ownership for one batch:

1. **Born:** `read_all_lines` rents the shelf and the strings
   (`strndup`), then *transfers ownership to the caller* through the
   out-parameter `*lines_out = lines;` (`cmd_sort.c:138`) — Chapter
   1's `***` pattern carrying a deed, not just data.
2. **Contents transferred:** `memcpy(lines + total, batch, ...)`
   copies the *Pips* (not the strings!) onto the master shelf. The
   strings now belong to `lines`.
3. **Shell of a shelf returned:** `free(batch)` — the temporary shelf
   goes back to the Clerk. The strings live on, owned by `lines`.

That sequence embodies the **container/contents law**: *freeing a
shelf of Pips never frees the boxes they point to.* Your code honors
the law in both directions — at the end of `sort_run`, contents first,
container second:

```c
        free(lines[i]);                /* each string...      cmd_sort.c:282 */
```
```c
    free(lines);                       /* ...then the shelf   cmd_sort.c:287 */
```

### Free-then-NULL: the `-u` idiom

Deduplication frees a string *early*, while the shelf still holds its
slot — and immediately ties the slot's Pip to nothing:

```c
            if (prev && sort_cmp(&lines[i], &prev, &ctx) == 0) {
                free(lines[i]);
                lines[i] = NULL;
            }
```
*(`apps/sort/cmd_sort.c:256-259`)*

Why the `= NULL`? Because the printing loop later walks every slot,
and `if (!lines[i]) continue;` (`cmd_sort.c:271`) must skip the dead
ones — and because the *final* free pass would otherwise free those
strings a **second time**. A double-free hands the Clerk a key he
already took back; his ledger now lists the box as free twice, and
glibc typically aborts the process on the spot (`double free or
corruption`). Free-then-NULL converts that catastrophe into a no-op,
because `free(NULL)` does nothing. Cheap insurance, structurally
applied.

(The cousin bug — *using* a freed box — is nastier: the Clerk doesn't
erase contents, so the ghost furniture is often still there and reads
"work"… until the box is re-rented to someone else. Chapter 1 §1.10
met this as the dangling tray; here it's the dangling box, and §3.6's
auditor catches both.)

### Every exit door passes the ledger desk

The user-visible discipline in your codebase: **count the doors out of
a function; every one of them must settle the ledger.** Two exhibits:

- `cat_run` has three doors — `--help` (`cmd_cat.c:158-161`), parse
  error (`:162-167`), and the bottom (`:206`) — and each one calls
  `arg_freetable(tbl, 6)` on the way out. Recall the geometry from
  §1.5: `tbl` itself is a stack tray of Pips (it dies free of charge),
  but the **argtable objects those Pips hold are rented heap boxes** —
  `arg_freetable` is the ledger desk that returns all six keys at
  once.
- `fetch_run` is the masterclass: it has *eight* error doors after
  resources start accumulating, and each settles exactly what exists
  at that point — look again at the §3.3 quote: `free(reply); close(fd);
  arg_freetable(tbl, 6); return 1;`. The reply box, the socket, the
  argtables: three different kinds of ownership, all settled, in one
  door. (Even the little request `frame` is freed on both its doors —
  `cmd_fetch.c:335` and `:339`.)

And one contract you don't own but must obey: **`getline` rents, the
caller returns.** `cat_plain` settles with `free(line)`
(`cmd_cat.c:82`); `read_all_lines` with `free(buf)` (`cmd_sort.c:137`).
The man page is the lease — `man getline` says, in effect, "the buffer
is `malloc`'d; you free it."

### The honest audit

Happy paths: all three components are leak-free. The OOM paths,
audited without mercy:

| Path | Settles | Verdict |
|---|---|---|
| `fetch` recv OOM (`cmd_fetch.c:372-377`) | old `reply` (proxy preserved it!), socket, argtables | ✅ exemplary |
| `read_all_lines` OOM (`cmd_sort.c:132`) | getline buffer, the shelf | ⚠️ the already-`strndup`'d **strings** are orphaned — contents of a freed container |
| `APPEND_LINES` outer OOM (`cmd_sort.c:222-227`) | `batch` shelf, argtables | ⚠️ `batch`'s strings *and* the master `lines` + contents are orphaned |
| `cat_json` OOM (`cmd_cat.c:98-99`) | nothing — the only key was overwritten | ❌ the trap (§3.4) |

This is the realistic texture of working C: perfect discipline on the
paths that run a million times, small honest debts on the
starving-warehouse paths that may never run once. The reason to fix
them anyway, in this project, is structural: an in-process shell
(R13) has no Mirror to die and take the leaks with it.

---

## 3.6 Valgrind — the auditor who reads the ledger out loud

You can reason about ownership; **valgrind observes it.** Memcheck
(valgrind's default tool) runs your binary on a synthetic CPU,
interposes on the Clerk's counter, and tracks every byte of the
warehouse: rented by whom (with a full stack trace), initialized or
not, freed when, touched by which instruction. At exit it reads the
ledger:

| Verdict | Meaning | Your mental model |
|---|---|---|
| **definitely lost** | a rented box with *no key anywhere* | `cat_json`'s trap, had it fired; any forgotten `free` |
| **indirectly lost** | reachable *only through* a lost box | the orphaned strings of a lost shelf — §3.5's audit table, literally: lose `lines`, and every `lines[i]` string becomes indirectly lost |
| **still reachable** | a global/static Pip still holds the key at exit | usually libc bookkeeping; benign noise |
| **Invalid read/write** | touching a box after `free`, or outside its bounds | ghost furniture; `reply[-1]` |
| **Invalid free** | returning a key twice, or a key the Clerk never issued | the double-free that free-then-NULL prevents |

The bar for your code: **`definitely lost: 0`** (and no invalid
accesses). Still-reachable noise from libc is normal and ignorable.

One sobering limitation, which ties this chapter together: valgrind
audits **what ran**, not what could run. A normal `cat --json` never
enters the OOM branch, so the §3.4 trap is invisible to it — dynamic
tools cannot exonerate your error paths. For those: eyes (this
chapter), code review, or static analysis (`gcc -fanalyzer` flags
several leak-on-error patterns at compile time). Use both kinds of
tool; they catch disjoint sins.

---

## 3.7 Lab — audit rahulbox with your own eyes

### Lab 1 — a clean bill of health

```sh
make
printf 'cat --json Makefile\nsort -u -k 1 Makefile\nexit\n' | \
    valgrind --leak-check=full ./shell/mysh
```

This exercises `cat_json`'s growth loop, `read_all_lines`,
`APPEND_LINES`, `qsort_r`, the `-u` free-then-NULL pass, the argtable
ledger desks — *and*, because input is a pipe, Chapter 2's `g_cmd_fd`
machinery. Expected tail: `definitely lost: 0 bytes` (the bar), ideally
`All heap blocks were freed -- no leaks are possible`. Note the run is
10–30× slower than native — the auditor checks every instruction;
that's the deal.

### Lab 2 — break it on purpose, read the report

Temporarily delete `free(line);` at `apps/cat/cmd_cat.c:82`, rebuild,
rerun Lab 1. The report now contains your first leak, with the full
chain of custody:

```
  120 bytes in 1 blocks are definitely lost in loss record 1 of 1
     at 0x....: realloc (vg_replace_malloc.c)
     by 0x....: getline (...)
     by 0x....: cat_plain (cmd_cat.c:67)
     by 0x....: cat_run (cmd_cat.c:181)
     ...
```

Read it bottom-up like Chapter 1's tower: who called whom, ending at
the counter where the unreturned box was rented. This stack — *the
allocation site, not the leak site* — is how you debug every leak you
will ever have. **Revert the edit.**

### Lab 3 — watch the auditor catch ghost furniture

Compile this six-liner and run it under valgrind:

```c
#include <stdlib.h>
int main(void)
{
    char *p = malloc(16);
    free(p);
    return p[3];              /* use-after-free: reading ghost furniture */
}
```

`Invalid read of size 1 ... inside a block of size 16 free'd` — with
*two* stacks: where you touched it, and where it was freed. Compare
how silently the same program runs natively. That silence is why
use-after-free survives testing; the auditor exists to end the
silence.

### Lab 4 — the trap is invisible to the auditor (and that's the lesson)

Run Lab 1 again and notice: no complaint about `cmd_cat.c:98`, the
`realloc` trap, ever — the OOM branch never executed. Now apply the
proxy fix as an exercise:

```c
            char *bigger = realloc(buf, cap);
            if (!bigger) {
                fprintf(stderr, "cat: out of memory\n");
                free(buf);                    /* old key still valid! */
                return 1;
            }
            buf = bigger;
```

Rebuild, rerun Lab 1 — output identical, valgrind identical. The fix
is invisible to every tool you have, and correct anyway. Some
discipline is enforced only by understanding; that is what this
chapter was for.

---

## 3.8 Chapter 3 — the rules to keep

> **R15.** The heap is lifetime-by-ledger: `malloc` rents, `free`
> returns *exactly the issued key, exactly once*. The Clerk's ledger
> entry sits just before your box — never write before byte 0.
> `realloc(NULL, n)` = `malloc(n)`; `free(NULL)` = no-op.
>
> **R16.** A growable buffer is the trio (pointer/capacity/length)
> plus **geometric** growth. Never grow by a constant: any `+k` is
> O(N²); any `×factor` is O(N), total copying < N — amortized, every
> byte moves less than one extra time.
>
> **R17.** Never `p = realloc(p, n)` — on failure your only key is
> overwritten and the still-rented box leaks. Proxy first
> (`tmp`/`grown`), check, then commit. (`sort` ✓, `fetch` ✓,
> `cat_json` ✗ — now fixed by you.)
>
> **R18.** Every box has exactly one owner; transfers are explicit
> (out-params like `*lines_out`, `memcpy` of Pips + free the old
> shelf). The container/contents law: freeing a shelf never frees what
> it points to — contents first, container second.
>
> **R19.** Count the doors: every exit path settles the ledger —
> `arg_freetable` at all three of `cat_run`'s doors, the
> `free + close + arg_freetable` triple at every one of `fetch_run`'s
> eight. APIs state who frees (`getline` rents, caller returns).
>
> **R20.** Free-then-NULL (`sort -u`) turns double-free into a no-op
> and use-after-free into a clean NULL crash. Freed boxes keep their
> ghost furniture — reads "work" until the box is re-rented.
>
> **R21.** valgrind audits what *ran*: `definitely lost: 0` is the
> bar; *indirectly lost* = contents of a lost container. It cannot see
> un-executed OOM branches — error paths are audited by eyes and
> `-fanalyzer`, not by runtime tools. And in an in-process shell
> (R13), leaks are forever: the discipline is structural.

---

## ⏭ Coming up in CHAPTER 4 — *Function Pointers, Central Registries, and the Inside-Out Anatomy of argtable3*

Pip has pointed at bytes, shelves, and boxes. Next he points at
**machines** — addresses in the `.text` district. The declaration
Chapter 1 deferred, `int (*run)(int, char **, FILE *, FILE *)`,
decoded for good; how `spec->run(...)` compiles to a single indirect
call; the registry as a 64-slot jump table and why `reg_find` +
function pointers *are* your shell's dispatch system; `qsort_r`
revisited from the other side (passing a machine to a machine); and
the design payoff — why `help` calling `print_usage` through the
struct means your usage text can never lie.

---
---

# CHAPTER 4
# Function Pointers, Central Registries, and the Inside-Out Anatomy of argtable3

> *In which Pip learns to point at machines instead of boxes, fifteen
> structs check into a card catalog, `help` writes its own user
> interface — and argtable3, opened up, turns out to be built from your
> own anatomy.*

---

## 4.1 Pip points at a machine

Chapter 1 promised that one declaration would be "Chapter 4's opening
act." Here it is — the two fields of `cmd_spec_t` we have so far taken
on faith:

```c
    int  (*run)(int argc, char **argv, FILE *in_stream, FILE *out_stream);
    void (*print_usage)(FILE *out);
```
*(`include/cmd_spec.h:10-11`)*

Apply the Chapter 1 rule — **declaration mirrors use**: "`(*run)(argc,
argv, in, out)` is a call returning `int`." So `run` is a Pip whose
string, when *jumped into* with those four arguments, produces an
`int`. The parentheses around `*run` are load-bearing: without them,
`int *run(...)` declares a *function returning `int *`* — the `()`
binds tighter than `*`, so you must parenthesize to say "pointer first,
function second."

What sits at the end of this string? Not numbers. **Instructions.**
Remember the §1.1 map: machine code lives in the `.text` district, and
an address is just a number — so nothing stops a Pip from pointing
there:

```
   cmd_cat_spec.run  (8 bytes, .data)          .text district
  ┌──────────────────┐               ┌─────────────────────────────┐
  │ 0x000055…41c0    │ ●━━━━━━━━━━━▶ │ cat_run:                    │
  └──────────────────┘               │    push  %rbp               │
   a machine Pip: walking            │    mov   %rsp,%rbp          │
   this string is not a              │    sub   $0x68,%rsp         │
   read — it is a JUMP               │    …                        │
                                     └─────────────────────────────┘
```

A data Pip is *walked* (the CPU loads or stores at the address). A
machine Pip is *jumped into* (the CPU sets its instruction pointer to
the address and keeps executing). The CPU has both call flavors baked
in: a **direct call** — `call cat_run` — has its destination glued in
at link time and can never change; an **indirect call** — `call *%rax`
— takes its destination *from data*, decided at runtime. Function
pointers exist to give you the second kind. Everything else in this
chapter is consequences.

The familiar conveniences carry over from Chapter 1, with a twist:

- **Function names decay**, exactly like array names (§1.6): in
  `.run = cat_run` (`cmd_cat.c:220`) the name becomes a pointer
  automatically — `&cat_run` is legal and identical.
- **Calls auto-dereference**: `spec->run(...)` and `(*spec->run)(...)`
  are the same call. Your code uses the clean spellings of both.
- **No arithmetic, no writing.** `run + 1` is meaningless (instructions
  are variable-length; C forbids it), and the `.text` district is
  read-only. Machine Pips are jump-only.
- **NULL is still the crater.** Calling a NULL function pointer jumps
  to address 0 — instant SIGSEGV. (Your registry never stores
  half-filled specs, so your code doesn't need to check; now you know
  what you're *not* checking for.)

Since every field of `cmd_spec_t` is a Pip — three to `.rodata`
strings, two to `.text` machines — the whole struct is five 8-byte
boxes, and its layout is worth knowing by heart, because §4.3 will
show it to you in disassembly:

```
        cmd_cat_spec  (.data district — 40 bytes total)
   offset  0: name         ●──▶ .rodata  "cat"
   offset  8: summary      ●──▶ .rodata  "concatenate files and…"
   offset 16: long_help    ●──▶ .rodata  "Concatenate FILE(s)…"
   offset 24: run          ●──▶ .text    cat_run           ← machines
   offset 32: print_usage  ●──▶ .text    cat_print_usage   ← machines
```

---

## 4.2 The machine-Pips you already own

Before the registry, take inventory: your codebase already hands
machines around as data in four places besides `cmd_spec_t` —

| Machine Pip | Handed to | Meaning |
|---|---|---|
| `.run = cat_run`, `.print_usage = cat_print_usage` (`cmd_cat.c:220-221`) | the registry → `run_inproc` | *dispatch*: the shell calls commands it cannot name at compile time (§4.3) |
| `sort_cmp` → `qsort_r(lines, total, sizeof(char *), sort_cmp, &ctx)` (`cmd_sort.c:250`) | glibc | *callback*: the library owns the loop, you inject the policy |
| `stage_thread_fn` → `pthread_create(&spawned[i].tid, NULL, stage_thread_fn, a)` (`mysh.c:394`) | pthreads | *"run this machine in a new Speedrunner"* (Chapter 5) |
| `raw_disable` → `atexit(raw_disable)` (`linedit.c:87`) | libc's exit machinery | *"call me when the room is demolished"* (Chapter 2 met this one) |

The `qsort_r` and `pthread_create` rows share a shape worth naming.
Chapter 1 §1.9 examined `&ctx` from the data side; now complete the
picture: both APIs take a **pair** — one machine Pip, one data Pip —

```
   qsort_r(...,        sort_cmp,        &ctx)
   pthread_create(..., stage_thread_fn, a)
                       ↑ the machine    ↑ the machine's luggage
```

That `(function, void *context)` pair is **C's idiom for a closure**:
code plus the state it should run with, traveling together. This is
*inversion of control* — normally you call the library; with
callbacks, the library calls *you*, and the `void *` is how your state
survives the round trip through code that has no idea what it carries.

And the `atexit` row teaches by *omission*: its signature is
`atexit(void (*fn)(void))` — a machine with **no luggage slot**. Which
is precisely why linedit's terminal state had to become file-scope
globals:

```c
static struct termios g_orig_termios;  /* saved settings, valid while g_raw */
static int            g_raw    = 0;    /* raw mode currently active         */
static int            g_raw_fd = -1;
```
*(`shell/linedit.c:67-69`)*

`raw_disable` can carry nothing in, so everything it needs must be
findable globally. A missing `void *ctx` in a 1970s API signature is
the direct cause of three globals in your 2026 shell — calling
conventions are destiny.

---

## 4.3 Anatomy Invocation — from token shelf to indirect call

Now the path the user asked about: how a line you type becomes a call
into `cat_run` **with no fork anywhere**. Eight hops, each one a line
you have already met — this section just connects them end to end.

**Hop 1 — read.** `linedit_read(input, prompt, line, sizeof line)`
(`mysh.c:607`) fills `main`'s 4 KB tray buffer (§1.6).

**Hop 2 — tokenize.** `tok_split(line, &tok, last_status)`
(`mysh.c:662`) cuts the line into a shelf of heap words (Chapter 7).

**Hop 3 — shape.** `parse_pipeline(&tok, stages, &nstages)`
(`mysh.c:677`) builds the NULL-fenced argv shelf of §1.3:
`stages[0].argv = { "cat", "-n", "notes.txt", NULL }`, `argc = 3`.

**Hop 4 — choose a strategy.** `last_status = run_pipeline(stages,
nstages)` (`mysh.c:767`) → single stage, so: `if (nstages == 1) return
run_inproc(&stages[0]);` (`mysh.c:270`).

**Hop 5 — resolve.** The string the user typed becomes a card:

```c
    cmd_spec_t *spec = reg_find(s->argv[0]);
```
*(`shell/mysh.c:199`)*

`argv[0]` — `"cat"` — is looked up in the registry (§4.4), which
returns `&cmd_cat_spec`, or `NULL`, in which case the Forking Mirror
takes over (Chapter 2). **This line is the fork in the road that
decides whether a fork happens.**

**Hop 6 — bind streams.** The anatomy's I/O contract from §2.4:

```c
        FILE *in  = stdin;
        FILE *out = stdout;
```
*(`shell/mysh.c:203-204`)*

…with redirections, if any, `fopen`'d over the defaults
(`mysh.c:208-223`).

**Hop 7 — FIRE.**

```c
        int rc = spec->run(s->argc, s->argv, in, out);
```
*(`shell/mysh.c:225`)*

Compiled, this is: *load 8 bytes from `spec + 24`* (offset of `run` —
§4.1's layout), *place four arguments per the calling convention*,
*`call` through the register*. One extra memory load versus a direct
call — **the entire runtime price of your shell's flexibility** (Lab 2
shows you the instruction). And dwell on what this line does *not*
know: at compile time, `run_inproc` has no idea whether it will run
`cat`, `sort`, or `fetch`. The destination was decided by *data* — a
string typed at a prompt, matched to a card, holding a machine Pip.
The textbook name is **late binding**; the practical name is: you can
add a sixteenth command without touching this line.

**Hop 8 — settle.** `fflush(out)`, close any owned redirection files
(`mysh.c:226-229`), and `rc` rides the §2.3 circuit into `$?` and the
prompt.

One more place fires the same field — the pipeline thread:

```c
    stage_arg_t *a = vp;
    a->exit_code = a->spec->run(a->argc, a->argv, a->in, a->out);
```
*(`shell/mysh.c:189-190`)*

Same interface, different executor (a Speedrunner — Chapter 5). That
is what interfaces buy: the *call* doesn't care who's calling.

> **The shape has a famous name.** A struct of function pointers,
> implemented per module, dispatched through at runtime: that is how
> C++ virtual calls work under the hood, and — closer to home — it is
> *literally* the Linux kernel's architecture: every device driver
> fills in a `struct file_operations { .open, .read, .write, … }` and
> the kernel calls through it without knowing which driver it's
> talking to. Your `cmd_spec_t` is polymorphism in C, the same
> technique that runs the operating system underneath you.

---

## 4.4 The Central Registry — a card catalog in `.bss`

Here is the entire dispatch infrastructure of your shell — twenty
lines, no magic:

```c
#define REGISTRY_MAX 64
static cmd_spec_t *registry[REGISTRY_MAX];
static int         registry_n = 0;

static void reg_register(cmd_spec_t *s)
{
    if (registry_n < REGISTRY_MAX) registry[registry_n++] = s;
}

static cmd_spec_t *reg_find(const char *name)
{
    for (int i = 0; i < registry_n; i++)
        if (strcmp(registry[i]->name, name) == 0) return registry[i];
    return NULL;
}

static void reg_print_all(void)
{
    for (int i = 0; i < registry_n; i++)
        printf("  %-16s %s\n", registry[i]->name, registry[i]->summary);
}
```
*(`shell/mysh.c:72-92`)*

Walk it with Chapter 1–3 eyes:

**The storage.** `registry` is an array of 64 *cards* — Pips to
`cmd_spec_t` — living in the `.bss` district (§1.1 map). `.bss` means
zero-initialized for free: at startup, all 64 cards are already NULL
and `registry_n` is already 0 — a valid empty catalog with **zero
lines of initialization code**. The district did the work.

**Registration appends cards, never copies.** Each app module compiled
its spec into its own `.data` (§1.2's diagram of `cmd_cat_spec`); the
shell declares them `extern` and files one 8-byte card apiece at
startup:

```c
extern cmd_spec_t cmd_hello_spec;
extern cmd_spec_t cmd_ls_spec;
extern cmd_spec_t cmd_cat_spec;
```
*(`shell/mysh.c:39-43`, fifteen in all)*
```c
    reg_register(&cmd_hello_spec);
    reg_register(&cmd_ls_spec);
```
*(`shell/mysh.c:534-548`, fifteen in all)*

`&cmd_cat_spec` is §1.8's pass-a-global-by-address, now with a job:
the linker stitched fifteen modules' `.data` symbols into one binary
(each module also ships as a standalone binary and a `.a` library —
same object code, two homes), and `main` files the cards:

```
   .bss district                        .data district (one per module)
   registry[64]
   ┌──────┐
   │ [0]  ●──▶ cmd_hello_spec { "hello", …, hello_run, … }
   │ [1]  ●──▶ cmd_ls_spec    { "ls",    …, ls_run,    … }
   │  ⋮   │                ⋮
   │ [14] ●──▶ cmd_fetch_spec { "fetch", …, fetch_run, … }
   │ [15] │ = NULL  ┄┄ 49 empty slots (free, courtesy of .bss)
   └──────┘     registry_n == 15
```

**Lookup is a linear scan** — fifteen `strcmp`s worst case, which at
this scale costs less than the time you spent reading this sentence. A
hash table would win at n = 10,000 and be unjustifiable ceremony at
n = 15. Choosing the simplest structure that meets the load *is* the
engineering, not a shortcut. (The same pattern, in its original
classroom form, lives in `apps/hello/registry.c:10-22` — your reference
implementation; the shell's copy just renames it.)

**And `help` builds its UI out of the catalog.** The built-in
(`mysh.c:741-746`) calls:

```c
static void print_help(void)
{
    printf("Built-in commands:\n");
    printf("  %-16s %s\n", "cd [DIR]",   "change working directory (default: $HOME)");
    printf("  %-16s %s\n", "exit [N]",   "exit the shell with optional status N");
    printf("  %-16s %s\n", "help",        "show this help");
    printf("\nRegistered commands:\n");
    reg_print_all();
    printf("\nAll other commands are looked up in PATH.\n");
}
```
*(`shell/mysh.c:476-485`)*

`reg_print_all` walks the same cards `reg_find` dispatches through and
prints each spec's `name` and `summary`. The consequence deserves
italics: ***the help screen is generated from the dispatch table
itself.*** There is no list of commands to keep updated, anywhere. The
day you add a sixteenth command, the entire integration is:

1. `extern cmd_spec_t cmd_tree_spec;`
2. `reg_register(&cmd_tree_spec);`
3. link the module in.

…and `help` already lists it, `reg_find` already finds it, `tree
--help` already works (via `print_usage` — next section), the pipeline
can already thread it. Three lines, **zero documentation edits**. (Lab
4's exercise has you do exactly this.)

One subtlety: `help` must be a *built-in* — not because it mutates the
room (R12), but because the catalog is room-private `.bss`. An exec'd
helper program would inherit fds and environment, but never the
registry: clones get a copy at flash-time, costume changes demolish
it, and a different binary never had it. Reading room-private state
binds you to the room as surely as writing it does.

---

## 4.5 The Single Source of Truth — argtable3 from the outside

First, the disease this design cures. The traditional hand-rolled CLI
has two parallel artifacts: a parsing loop (`getopt` switch) and a
usage string (`"usage: cat [-n] …"`), written separately, maintained
separately. They *always* drift: a flag gets added to the switch but
not the help; the help promises `--force` that was renamed last year.
The documentation doesn't merely go stale — it starts to *lie*.

Your anatomy makes the lie structurally impossible, and here is the
whole mechanism. One function per module *builds a table that
describes the interface* — options as data:

```c
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *number    = arg_lit0("n", "number",    "number all output lines");
    *show_ends = arg_lit0("E", "show-ends", "display $ at end of each line");
    *json      = arg_lit0(NULL, "json",     "emit machine-readable JSON (for agents/MCP)");
    *files     = arg_filen(NULL, NULL, "[FILE...]", 0, 64, "files to concatenate (default: stdin)");
    *end       = arg_end(20);
```
*(`apps/cat/cmd_cat.c:21-26` — inside `build_cat_argtable`, Chapter 1's
out-parameter builder, renting Chapter 3 heap boxes)*

And **two different callers project two different views of that one
table**:

```
                     build_cat_argtable()          ← the ONE source
                      /                  \
            cat_run()                     cat_print_usage()
            arg_parse(argc, argv, tbl)    arg_print_syntax(out, tbl, "\n")
            fills counts & values         arg_print_glossary(out, tbl, …)
            ═ THE PARSER                  ═ THE DOCUMENTATION
```

View one — parsing (`cmd_cat.c:153-161`):

```c
    build_cat_argtable(&help, &number, &show_ends, &json, &files, &end, tbl);

    int nerrors = arg_parse(argc, argv, tbl);

    if (help->count > 0) {
        cat_print_usage(out_stream);
        arg_freetable(tbl, 6);
        return 0;
    }
```

`arg_parse` walks `argv` against the table, incrementing `count`
fields and collecting values (`files->filename[i]`); unrecognized
flags accumulate as errors in `arg_end`. Note the elegance of the
`--help` flag itself: it's just another row of the table, checked by
`help->count > 0` — and its handler *is the other view*.

View two — documentation (`apps/cat/cmd_cat.c:125-140`):

```c
void cat_print_usage(FILE *out)
{
    struct arg_lit  *help, *number, *show_ends, *json;
    struct arg_file *files;
    struct arg_end  *end;
    void            *tbl[7];

    build_cat_argtable(&help, &number, &show_ends, &json, &files, &end, tbl);

    fprintf(out, "Usage: cat ");
    arg_print_syntax(out, tbl, "\n");
    fprintf(out, "\nConcatenate files and print to standard output.\n\nOptions:\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");

    arg_freetable(tbl, 6);
}
```

Same builder, same table — but rendered instead of parsed:
`arg_print_syntax` derives the `Usage: cat [-hnE] [--json] [FILE...]`
line from the option definitions; `arg_print_glossary` prints each
option's flags and glossary string in columns. So the single line

```c
    *number = arg_lit0("n", "number", "number all output lines");
```

is *simultaneously* the parser rule for `-n/--number`, the token in
the syntax line, and the row in the glossary. Change it once; all
three surfaces change. Drift isn't discouraged — it has **no place to
live**.

Precision matters on one point: single **source** does not mean single
**instance**. `cat_run` and `cat_print_usage` each build a *fresh*
table — separate heap rentals, separately freed (count the
`arg_freetable`s). That is deliberate, and your `fetch` module wrote
down why:

> *"Stack-allocated builder pattern: every argtable handle lives in
> the caller's frame and is passed in by address, so
> `build_fetch_argtable()` owns no static/global state. Two threads
> may each hold their own set of handles and call
> `run()`/`print_usage()` concurrently without interfering."*
> *(`apps/fetch/cmd_fetch.c:25-28`)*

Per-call instances are what make the anatomy **thread-safe** — when
Chapter 5 runs `sort` and `grep` simultaneously on two Speedrunners,
each owns its own table. One source, many instances, zero shared
mutable state: that triple is the whole design.

---

## 4.6 …and from the inside: the anatomy all the way down

Open the library underneath all of this — your vendored
`argtable3.h` — and look at what every `arg_lit`, `arg_int`,
`arg_file` *begins with*:

```c
typedef struct arg_hdr {
    char flag;             /**< Modifier flags for this option … */
    const char* shortopts; /**< String listing the short option characters (e.g., "hv") */
    const char* longopts;  /**< String listing the long option names … */
    const char* datatype;  /**< Description of the argument data type … */
    const char* glossary;  /**< Description of the option as shown in the glossary/help output */
    /* … mincount, maxcount, parent … */
    arg_resetfn* resetfn;  /**< Pointer to the type-specific reset function … */
    arg_scanfn* scanfn;    /**< Pointer to the type-specific scan (parsing) function */
    arg_checkfn* checkfn;  /**< Pointer to the type-specific validation function */
    arg_errorfn* errorfn;  /**< Pointer to the type-specific error reporting function */
    /* … */
} arg_hdr_t;
```
*(`vendor/argtable3/argtable3.h:277-291`, trimmed)*

Strings of metadata… followed by **four function pointers**. Each
option *type* supplies its own machines — how to reset itself, how to
scan one argument, how to validate, how to report errors:

```c
typedef int(arg_scanfn)(void* parent, const char* argval);
```
*(`vendor/argtable3/argtable3.h:152`)*

— and there's the `(machine, void *context)` closure pair *again*:
`parent` is the option's own struct, threaded back to it by the
library, exactly like `qsort_r`'s `vctx` and `pthread_create`'s `a`.
When `arg_parse` walks your table, it dispatches
`hdr->scanfn(hdr->parent, argval)` without knowing or caring whether
the option is a literal, an int, or a filename — **the identical shape
to `run_inproc` calling `spec->run` without knowing which command it
runs.** Even the table's end is marked the way `argv` is fenced:

```c
    ARG_TERMINATOR = 0x1, /**< Marks the end of an argument table (sentinel entry) */
```
*(`vendor/argtable3/argtable3.h:82`)*

Hold the two structs side by side:

| your `cmd_spec_t` | argtable3's `arg_hdr` |
|---|---|
| `name`, `summary`, `long_help` — metadata strings | `shortopts`, `longopts`, `datatype`, `glossary` |
| `run` — the do-it machine | `scanfn` + `checkfn` |
| `print_usage` — the describe-it machine | (the same `glossary`/`datatype` fields, rendered by `arg_print_*`) |
| registry array, NULL when empty | option table, `ARG_TERMINATOR` sentinel |
| `reg_find` → dispatch by name | `arg_parse` → dispatch by flag |

The pattern you built your shell on — *struct of metadata + function
pointers, filed in a table, dispatched without knowing the concrete
type* — is the same pattern your argument parser is built on, which is
the same pattern the kernel's drivers are built on. It is not a
classroom toy. It is how C does extensibility, at every scale, all the
way down.

---

## 4.7 Lab — watch a pointer become a call

### Lab 1 — three spellings, three districts

```sh
gdb -batch shell/mysh \
  -ex 'print cat_run' \
  -ex 'print &cat_run' \
  -ex 'print cmd_cat_spec.run' \
  -ex 'print &cmd_cat_spec' \
  -ex 'print cmd_cat_spec.name'
```

The first three print the *identical* value — `{int (int, char **,
FILE *, FILE *)} 0x… <cat_run>` — decay (`cat_run` ≡ `&cat_run`) and
the struct field, all one machine Pip. Then map the districts by
symbol table:

```sh
nm shell/mysh | grep -wE 'cat_run|cmd_cat_spec|registry'
```

Expect three letters: `T cat_run` (**t**ext — a machine), `D
cmd_cat_spec` (**d**ata — an initialized global), `b registry`
(**b**ss — zero-initialized; lowercase because `static` makes it
file-local). One line of `nm` output per district of the §1.1 map.

### Lab 2 — the indirect call, in the metal

```sh
gdb -batch -ex 'disassemble run_inproc' shell/mysh | grep 'call.*\*'
```

Expect something like `call *0x18(%rax)` — *"jump to whatever address
sits 0x18 bytes into the struct that `%rax` points at."* And 0x18 = 24
= `offsetof(cmd_spec_t, run)` — §4.1's layout table, confirmed by the
machine. Verify the offset yourself:

```c
#include <stdio.h>
#include <stddef.h>
#include "cmd_spec.h"
int main(void) {
    printf("run at offset %zu; struct is %zu bytes\n",
           offsetof(cmd_spec_t, run), sizeof(cmd_spec_t));
    return 0;
}   /* gcc -Iinclude probe4.c → "run at offset 24; struct is 40 bytes" */
```

### Lab 3 — ride a live dispatch

```sh
gdb shell/mysh
(gdb) break run_inproc
(gdb) run
mysh> cat Makefile
(gdb) next                 # step over reg_find
(gdb) print spec           # (cmd_spec_t *) 0x… <cmd_cat_spec>
(gdb) print *spec          # all five fields — gdb names both machines
(gdb) print spec->run      # {int (…)} 0x… <cat_run>
(gdb) advance cat_run      # ride the indirect call to its destination
(gdb) backtrace            # …cat_run ← run_inproc ← run_pipeline ← main
```

`print *spec` is the card catalog made visible; `advance cat_run`
lands you on the far side of the `call *0x18(…)` from Lab 2; the
backtrace is Chapter 1's tower, reached this time through a pointer.

### Lab 4 — drift is impossible: the one-edit test

In `build_cat_argtable`, change one glossary string —
`"number all output lines"` → `"number all output lines (v2)"` —
rebuild, and check **every surface at once**: `cat --help` (glossary
row changed), the syntax line (still consistent), and parse behavior
(unchanged, same flags). One edit, all projections. Now try to make
`cat --help` *lie* about a flag without touching parsing. You can't —
there is no second copy to corrupt. Revert when convinced.

### Exercise — the three-line extension, for real

Write `apps/demo/cmd_demo.c`, the smallest possible anatomy citizen:

```c
#include <stdio.h>
#include "argtable3.h"
#include "cmd_spec.h"

static void build_demo_argtable(struct arg_lit **help,
                                struct arg_end **end, void **tbl)
{
    *help = arg_lit0("h", "help", "show this help and exit");
    *end  = arg_end(20);
    tbl[0] = *help; tbl[1] = *end; tbl[2] = NULL;
}

void demo_print_usage(FILE *out)
{
    struct arg_lit *help; struct arg_end *end; void *tbl[3];
    build_demo_argtable(&help, &end, tbl);
    fprintf(out, "Usage: demo ");
    arg_print_syntax(out, tbl, "\n");
    arg_print_glossary(out, tbl, "  %-22s %s\n");
    arg_freetable(tbl, 2);
}

int demo_run(int argc, char **argv, FILE *in, FILE *out)
{
    (void)in;
    struct arg_lit *help; struct arg_end *end; void *tbl[3];
    build_demo_argtable(&help, &end, tbl);
    int nerrors = arg_parse(argc, argv, tbl);
    if (help->count > 0) { demo_print_usage(out); arg_freetable(tbl, 2); return 0; }
    if (nerrors > 0) {
        arg_print_errors(stderr, end, "demo");
        arg_freetable(tbl, 2); return 1;
    }
    fprintf(out, "demo: the anatomy lives!\n");
    arg_freetable(tbl, 2);
    return 0;
}

cmd_spec_t cmd_demo_spec = {
    .name        = "demo",
    .summary     = "prove the registry UI builds itself",
    .long_help   = "Minimal cmd_spec_t citizen, built in Chapter 4.",
    .run         = demo_run,
    .print_usage = demo_print_usage,
};
```

Then the three lines: `extern cmd_spec_t cmd_demo_spec;` in `mysh.c`,
`reg_register(&cmd_demo_spec);` in `main`, and the object added to the
shell's link (follow the house pattern of any `apps/*` Makefile).
Rebuild, run `help` — your command is listed, summary and all, by code
you never touched. Run `demo --help` — usage text you never wrote by
hand. *That* is the anatomy paying rent.

---

## 4.8 Chapter 4 — the rules to keep

> **R22.** A function pointer is a Pip into `.text`: declaration
> mirrors use (`int (*run)(…)` — the parens are load-bearing); names
> decay (`cat_run` ≡ `&cat_run`); calls auto-deref (`f()` ≡ `(*f)()`);
> no arithmetic, no writes; NULL is still the crater.
>
> **R23.** Dispatch costs one load plus one indirect call
> (`call *0x18(%rax)`) — the destination is *data*, decided at
> runtime. Late binding is what lets you add commands without touching
> the dispatcher.
>
> **R24.** The C closure is the pair `(machine Pip, void *context)` —
> `qsort_r(…, sort_cmp, &ctx)`, `pthread_create(…, stage_thread_fn,
> a)`. When an API omits the context slot (`atexit`), state is forced
> into globals — signatures are destiny.
>
> **R25.** An interface in C is a struct of function pointers; a table
> of them is a vtable you control. Code against the struct
> (`run_inproc`), implement per module (15 apps), bind by name at
> runtime (`reg_find`). The Linux kernel's `file_operations` is this
> exact shape.
>
> **R26.** Registries hold cards, not copies: 8-byte Pips to `.data`
> specs, in a `.bss` array that zero-initializes itself. A linear scan
> over 15 entries is correct engineering — size the structure to the
> load, not to the textbook.
>
> **R27.** Single source of truth: parsing, syntax line, and glossary
> must all be projections of one structure (`build_*_argtable`), so
> documentation *cannot* lie. But single source ≠ single instance —
> per-call tables (fetch's comment) are what make the anatomy
> thread-safe.

---

## ⏭ Next: CHAPTER 5 — *The Threaddy Speedrunners*

The Speedrunners have been cameoing since Chapter 1; now they take the
stage. How `run_pipeline` lays conveyor belts (`pipe()`) between
workers who share one room; why that's brutally efficient and exactly
as dangerous as it sounds; the data race that three concurrent
`arg_parse` calls would have on one shared bookmark — and the five
lines of `__thread` in your vendored argtable3 that cure it.

---
---

# CHAPTER 5
# The Threaddy Speedrunners: POSIX Threads, Concurrent Streams, and Thread-Local Storage

> *In which three workers parse three order sheets against one shared
> bookmark, `sort -rn` comes back ascending one run in fifty — and
> five lines of `__thread` in a vendored file hand every Speedrunner a
> private notepad.*

---

## 5.1 One room, many workers — what a thread actually is

The Forking Mirror duplicates the whole room. `pthread_create` does
something stranger: it hires **another worker into the same room**. A
new thread receives exactly three private things:

1. a fresh **note-pad** — its own stack (≈8 MB by default, following
   `ulimit -s`), allocated in the *mmap district* of the §1.1 map, not
   in the main STACK district;
2. its own **registers** (including the instruction pointer — it runs
   its own function);
3. its own **TLS block** — the per-worker annex §5.5 is about, where
   `errno` has always lived.

**Everything else is shared.** The heap, every global, every loaded
district, and — sharper than you expect — the **fd card-box itself**:
where a Mirror clone gets a *photocopy* of the card-box, Speedrunners
all reach into the *same one*. Side by side:

| | Forking Mirror clone | Threaddy Speedrunner |
|---|---|---|
| address space | copied (lazily, COW) | **shared** — same room |
| fd table | photocopied card-box | **the same card-box** |
| stack | a copy of the whole tower | own fresh note-pad |
| `errno` | own copy | own copy (TLS — §5.5) |
| can talk via | pipes, files, exit codes | *any byte of memory* |
| a crash | dies alone | **kills the whole room** |
| creation cost | flash + COW page tables (~100 µs–ms) | ~10× lighter |

Why your shell wants them: run `sort | uniq -c | sort -rn` in bash and
you pay **three forks and three execs**. In mysh, all three commands
are registry residents, so the same pipeline is **zero forks**: three
function calls (through `spec->run` machine Pips, R23) on three
note-pads, joined by two kernel pipes. That is the efficiency headline
of this chapter.

And the fine print: the room has **no walls inside it**. Every
efficiency above comes from sharing, and every danger below comes from
the same sharing. A Speedrunner who scribbles on a shared box
scribbles for everyone — and R13's bet ("a crash in the command kills
the whole shell") is now placed *three times concurrently*.

---

## 5.2 Threaded Pipeline Stream Architecture — the conveyor belts

Walk `run_pipeline` (`shell/mysh.c:268-472`) building our running
example, `sort | uniq -c | sort -rn` — three internal stages, three
Speedrunners. Four moves per stage.

### Move 1 — lay a conveyor belt between adjacent stages

```c
        if (!last) {
            /*
             * Create the inter-stage pipe.  O_CLOEXEC ensures that any
             * fork'd external-command child exec's with these fds closed,
             * preventing it from holding a write end open and blocking EOF.
             */
            int pfd[2];
            if (pipe(pfd) < 0) {
```
```c
            fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
            fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
            cur_out_fd = pfd[1];
            next_in_fd = pfd[0];
```
*(`shell/mysh.c:315-330`, trimmed)*

`pipe(pfd)` mints **two cards on one kernel conveyor belt**: `pfd[1]`
is the feed end (this stage's output), `pfd[0]` the take-off end (next
stage's input). The `FD_CLOEXEC` stamps are Chapter 8's story —
self-destruct-on-costume-change, so a fork'd external stage can't
accidentally hold a feed end open forever.

### Move 2 — give the endpoint stages safe cards

The first stage reads the shell's stdin; the last writes the shell's
stdout. But handing a Speedrunner *the* fd 0 card would be fatal —
its `fclose` would close the shell's real stdin. So:

```c
            } else if (spec) {
                /* Internal thread: dup stdin so fclose in the thread doesn't
                 * close the shell's real fd 0. */
                cur_in_fd = dup(STDIN_FILENO);
```
*(`shell/mysh.c:298-301`; the mirror-image `dup(STDOUT_FILENO)` at `:343-344`)*

Chapter 2's law — *dup copies cards, never bookmarks* — used as a
**safety device**: the worker gets its own card to close, the shell
keeps fd 0.

### Move 3 — wrap raw cards in private furniture, pack the luggage

```c
        if (spec) {
            /*
             * Internal command — run inside a worker thread.
             * fdopen() takes ownership of the fds: the thread closes them via
             * fclose() when it finishes, which signals EOF to the next stage.
             */
            stage_arg_t *a = malloc(sizeof *a);
```
```c
            a->spec      = spec;
            a->argc      = s->argc;
            a->argv      = s->argv;
            a->exit_code = 0;

            a->in = fdopen(cur_in_fd, "r");
```
```c
            a->out = fdopen(cur_out_fd, "w");
```
```c
            spawned[i].is_thread = 1;
            spawned[i].arg       = a;
            if (pthread_create(&spawned[i].tid, NULL, stage_thread_fn, a) != 0) {
```
*(`shell/mysh.c:357-394`, trimmed)*

Three chapters converge in twelve lines:

- **The packet is heap-rented** (`malloc(sizeof *a)` — the R7/§1.9
  decision finally fully explained: the spawning loop's locals keep
  changing while workers run; only the warehouse outlives the tray).
- **`fdopen` turns a raw card into private furniture** — a `FILE`
  with its own buffer, owned by exactly one Speedrunner. This is what
  "thread-isolated streams" means mechanically: nobody else holds
  these `FILE *`s, so there is nothing to fight over. (And recall the
  anatomy's signature takes `FILE *in, FILE *out` — designed, since
  Chapter 2, for exactly this hand-off.)
- **`pthread_create(&tid, NULL, stage_thread_fn, a)`** is R24's
  closure pair in the flesh: one machine Pip, one luggage Pip.

### Move 4 — the worker, and the EOF cascade

```c
static void *stage_thread_fn(void *vp)
{
    stage_arg_t *a = vp;
    a->exit_code = a->spec->run(a->argc, a->argv, a->in, a->out);
    fclose(a->out);   /* flush and signal EOF to the downstream stage */
    fclose(a->in);
    return a;
}
```
*(`shell/mysh.c:187-194`)*

Seven lines run every pipeline stage in your shell. The profound one
is `fclose(a->out)`. **EOF is not a character** — it is the kernel
observing that *zero feed-end cards remain* on a conveyor belt. When
stage 1's `sort` finishes and `fclose`s its feed end, stage 2's
`getline` — blocked mid-read — returns -1; `uniq -c` finishes,
`fclose`s *its* feed end; stage 3 drains and ends. Completion
propagates down the pipeline as a cascade of closing cards:

```
   the mysh room — one process, four workers

   main: lays belts A and B, hires three, then joins three

   T1 sort                 T2 uniq -c               T3 sort -rn
   in : FILE(dup fd 0)     in : FILE(A take-off)    in : FILE(B take-off)
   out: FILE(A feed)       out: FILE(B feed)        out: FILE(dup fd 1)
        │                       │                        │
        │ fclose ⇒ A EOF ──────▶│ fclose ⇒ B EOF ───────▶│ fclose ⇒ tty
        ▼                       ▼                        ▼
      done                    done                     done → exit code
```

Finally main collects everyone — and the comment explains an ordering
subtlety worth reading twice:

```c
    /*
     * Collect results.  Join all threads (including intermediate ones) before
     * reaping processes, so that thread-generated writes reach downstream
     * readers before we declare the pipeline done.
     *
     * pthread_join on the final thread gives us the terminal exit status.
     */
```
```c
        if (spawned[i].is_thread) {
            pthread_join(spawned[i].tid, NULL);
            int rc = spawned[i].arg->exit_code;
            free(spawned[i].arg);
```
*(`shell/mysh.c:450-462`, trimmed)*

`pthread_join` is the synchronization point this book has leaned on
since §1.10: it is what makes the packet's borrowed `argv` lease
legal (`tok_free` only runs after `run_pipeline` returns), and it
settles each packet's ledger (`free(spawned[i].arg)`, R19).

Tally the ownership map, because it *is* the thread-safety design:
each Speedrunner exclusively owns **its packet, its two `FILE`
streams, its argtable instances** (R27), and borrows its argv under a
join-guaranteed lease. Shared and mutable: *nothing*. Almost.

---

## 5.3 What the room shares — the risk inventory

Audit everything three concurrent stages can touch:

| Shared thing | Why it's safe (or isn't) |
|---|---|
| the heap | the Clerk's desk is internally locked (§3.1) — concurrent `malloc` is fine |
| `stdout`/`stderr` `FILE`s | POSIX requires stdio to lock per-`FILE` per-call: two threads' `fprintf(stderr, …)` may interleave by *line*, never by *torn bytes* |
| `errno` | per-thread **by definition** — the original TLS citizen (§5.5) |
| the fd card-box | shared — but each stage only ever touches cards it owns (Move 2's `dup` discipline). Safety by ownership, not by lock |
| the environment, the cwd | `setenv`/`chdir` happen only in built-ins — and the grammar confines built-ins to single-stage lines: `/* ── single-stage built-ins (must run in-process) ── */` (`mysh.c:682`). `cd`, `exit`, `VAR=value` can never ride a Speedrunner |
| **library-internal globals** | ⚠️ **the landmine.** Your code shares no mutable state — but code you *link* might |

That last row is the rest of the chapter. First, the formal definition
it turns on:

> **A data race** is: two workers touch the same box, at least one
> write, and nothing orders the touches. The C standard calls the
> result *undefined behavior* — not "wrong value," but "all bets,
> including the crash-shaped ones, are off."

---

## 5.4 The Data Race Catastrophe — one bookmark, three order sheets

`arg_parse` looks innocent — you hand it *your* argv and *your* table
(per-call instances, R27). But walk into the vendored amalgamation and
look at what the embedded getopt engine exports:

```c
extern __thread char *optarg;			/* getopt(3) external variables */
extern __thread int optind, opterr, optopt;
```
*(`vendor/argtable3/argtable3.c:1383-1384`)*

Ignore the `__thread` for a moment — that's the fix (§5.5), and it
wasn't always there. Upstream, these are plain globals, because
**getopt's 1974-vintage API contract *is* its globals**: `optind` (the
bookmark — index of the next argv slot to scan), `optarg` (Pip to the
current option's value), plus hidden engine state we'll meet shortly.
R24 said *signatures are destiny*; here, **linkage is destiny** — any
library that keeps its working state in globals makes every caller
single-threaded by default, transitively, without telling you.

Now run the counterfactual (Lab 3 makes it real): strip the patch, so
all of it is plain process-globals again, and type:

```
mysh> sort | uniq -c | sort -rn
```

Three Speedrunners start within microseconds; each immediately enters
the same scanning loop:

```c
    while ((copt = getopt_long(argc, argv, shortoptions, longoptions->options, NULL)) != -1) {
```
*(`vendor/argtable3/argtable3.c:5320`)*

Each `getopt_long` iteration reads and writes the shared engine state:
the bookmark advances (`optind++`, `:1664`), values are grabbed
relative to it (`optarg = nargv[optind++]`, `:1745`), and a scanning
pointer named `place` walks *byte-by-byte through the current flag
cluster* (`:1840`) — for T3 that means `place` points into its
`"-rn"`. One bookmark wall, three workers, three different order
sheets:

| t | T2 (`uniq -c`, argc 2) | T3 (`sort -rn`, argc 2) | shared boxes |
|---|---|---|---|
| 1 | sets `place` → `"c"` | — | `place="c"` |
| 2 | — | sets `place` → `"rn"` *(clobbers)* | `place="rn"` |
| 3 | resumes scanning `place`: sees `'r'` — **uniq has no `-r`** → "invalid option" | — | `optopt='r'` |
| 4 | `optind++` | `optind++` | `optind` skips a slot **for both** |
| 5 | — | bounds-check passed *before* t4's bump; now reads `nargv[optind]` **past its NULL fence** | wild Pip → 💥 |

The visible symptoms, mapped to mechanism:

- **Dropped flags** — the shared bookmark jumps past `"-rn"` before
  T3 ever scans it: stage 3 runs as a plain ascending `sort`; your
  "reverse numeric" output comes back forwards. No error printed.
- **Phantom flags** — T2 parses a cluster that T3 wrote into `place`:
  `uniq: invalid option -- r`, for an option nobody typed.
- **Argument loss, literally** — the engine's permute machinery
  reorders argv using the shared indices `nonopt_start`/`nonopt_end`
  (`:1506-1507`): one thread physically shuffles *its* argv using
  *another's* positions. Filenames vanish from the vector.
- **Crashes** — the time-of-check/time-of-use window at t5: the
  bounds check and the access straddle another thread's increment,
  and the read sails past the NULL fence into garbage Pips (§1.3's
  contract, violated by concurrency rather than by forgetting).

And the signature of the whole class: **it almost always works.** The
parse takes microseconds; the scheduler must interleave two of them
just so. One run in fifty is wrong, one in five thousand crashes, and
none of it reproduces under the debugger — the heisenbug. "It ran
fine 49 times" is not evidence against a data race; it is exactly
what a data race looks like.

---

## 5.5 The Thread-Local Fix — a notepad stapled to every worker

Now read the actual code in your tree — the patch, with its
documentation:

```c
/* These getopt globals are made thread-local (__thread) so that mysh can run
 * multiple argtable3-parsing commands concurrently, one per pipeline stage,
 * without the stages racing on the shared parser state.  Local patch to the
 * vendored amalgamation — re-apply after any argtable3 upgrade. */
__thread int	opterr = 1;		/* if error message should be printed */
__thread int	optind = 1;		/* index into parent argv vector */
__thread int	optopt = '?';	/* character checked for validity */
__thread int	optreset;		/* reset getopt */
__thread char *optarg;		/* argument associated with option */
```
*(`vendor/argtable3/argtable3.c:1466-1474`)*

And — the part a hasty fix would have missed — the engine's **hidden**
state, the `static`s no header ever mentions:

```c
static __thread char *place = EMSG; /* option letter processing */

/* XXX: set optreset to 1 rather than these two */
static __thread int nonopt_start = -1; /* first non option argument (for permute) */
static __thread int nonopt_end = -1;   /* first option after non options (for permute) */
```
*(`vendor/argtable3/argtable3.c:1503-1507`; also `dash_prefix` at `:1513`,
the error buffer `opterrmsg` at `:1542-1543`, and — subtlest of all — a
function-local `static __thread int posixly_correct` at `:1803`. A
`static` inside a function is still **one shared box**; `__thread`
fixes those too.)*

**What `__thread` does.** One keyword changes a variable's *storage
duration*: instead of one box in `.data`/`.bss`, the linker records a
**template** (initialized values in a section called `.tdata`,
zero-fill in `.tbss`), and every thread — at `pthread_create` time —
gets a *fresh copy of the template* in its own TLS block, allocated
alongside its note-pad. Accessing the variable compiles to one
segment-relative load (`mov %fs:offset, %eax` on x86-64) — the `%fs`
register points at *the current worker's* block, so the same line of
C touches a different box in every thread. No locks, no lookups,
nearly free. The §1.1 map gains a per-worker annex:

```
        T1's TLS block            T2's TLS block            T3's TLS block
        ┌──────────────┐          ┌──────────────┐          ┌──────────────┐
        │ optind = 1   │          │ optind = 1   │          │ optind = 1   │
        │ place ●──┐   │          │ place ●──┐   │          │ place ●──┐   │
        │ optarg…  │   │          │ optarg…  │   │          │ optarg…  │   │
        └──────────┼───┘          └──────────┼───┘          └──────────┼───┘
              into T1's argv           into T2's argv           into T3's argv
        same symbol, same line of C — three private boxes, zero sharing
```

Each Speedrunner now scans by **its own bookmark**, walks **its own**
`place` through **its own** flag cluster, permutes **its own** argv
with **its own** indices. The race isn't mitigated; the shared state
it needed *no longer exists*. (Note the one rule TLS imposes:
initializers must be compile-time constants — `= 1`, `= '?'`, `= EMSG`
all qualify. And you have been using TLS your whole C life: POSIX
made `errno` per-thread decades ago for exactly this reason; the
patch extends `errno`'s courtesy to `optind`.)

**Why TLS and not a mutex?** Three reasons, worth generalizing:

1. **A lock can't fix this API.** The state leaks out per iteration —
   callers read `optarg`/`optind` *between* `getopt_long` calls — so
   you'd have to hold one big lock across the entire `arg_parse`,
   serializing all parsing… and any code touching `optind` outside
   the lock still races.
2. **The contract is the globals.** `optind` is declared `extern` in
   the public header (`:1383-1384`); TLS preserves every caller's
   source code unchanged — same names, same reads — and changes only
   *where the boxes live*. A reentrant redesign (`getopt_r`) would
   change the API; a lock would change the performance; `__thread`
   changes five declarations.
3. **It's free.** One segment-prefixed load. The parse stays fully
   parallel.

Step back and admire the full defense-in-depth your shell now carries,
layer by layer, each from a different chapter:

| Layer | Mechanism | Chapter |
|---|---|---|
| handles | per-call argtables — fresh instances per `run()` | R27 |
| streams | per-stage `fdopen`'d `FILE`s — owned, never shared | §5.2 |
| borrows | `pthread_join` before `tok_free` — leases honored | §1.10 |
| room state | built-ins confined to single-stage lines | §5.3 |
| library state | `__thread` on every byte of parser state | §5.5 |

…and the punchline, verified against your tree: **there is not a
single `pthread_mutex` in the entire codebase.** Not because locking
is bad — because the architecture partitioned ownership until nothing
shared was left to lock. (When you *do* need one — genuinely shared,
genuinely mutated state — `pthread_mutex_lock` is the talking stick:
one worker speaks at a time. mysh simply arranged never to need the
stick.) And the `fetch` comment quoted in §4.5 — *"Two threads may
each hold their own set of handles and call run()/print_usage()
concurrently without interfering"* — is only an honest sentence
because of the five `__thread` lines above. The comment made a
promise; the patch keeps it.

---

## 5.6 Lab — race the Speedrunners yourself

### Lab 1 — count the workers

```sh
./shell/mysh
mysh> cat | grep x | wc -l        # cat waits on the tty: pipeline parked
```
From another terminal:
```sh
ls /proc/$(pgrep -x mysh)/task    # 4 entries: main + three Speedrunners
```
Each directory is a thread ID — the kernel's view of your hires. Back
in mysh, type a line containing `x`, then Ctrl-D, and watch the EOF
cascade finish the count.

### Lab 2 — same symbol, three boxes

```c
#include <stdio.h>
#include <pthread.h>

__thread int notepad;

void *worker(void *name)
{
    notepad = 7;
    printf("%-6s notepad at %p = %d\n", (char *)name, (void *)&notepad, notepad);
    return NULL;
}

int main(void)
{
    pthread_t a, b;
    notepad = 1;
    pthread_create(&a, NULL, worker, "left");
    pthread_create(&b, NULL, worker, "right");
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("%-6s notepad at %p = %d\n", "main", (void *)&notepad, notepad);
    return 0;
}   /* gcc -pthread tls_probe.c && ./tls_probe */
```

Three different addresses for one symbol — and `main` still prints
`1`: the workers' `7`s were written into notepads that died with
them. That is TLS in four printf lines.

### Lab 3 — resurrect the race (and put it back)

Strip the patch, exactly as a stock argtable3 would be:

```sh
sed -i 's/^__thread //; s/static __thread /static /' vendor/argtable3/argtable3.c
make
printf '3\n1\n2\n2\n' > /tmp/nums
for i in $(seq 100); do
  printf 'sort -n /tmp/nums | uniq -c | sort -rn\n' | ./shell/mysh
  echo ---
done | sort | uniq -c | sort -rn
```

With the patch, 100 identical blocks. Without it, *most* runs are
still identical — and a few are not: orderings that ignore `-rn`,
stray `invalid option` complaints, possibly a crash. Races are shy;
that shyness is the lesson (raise the loop count, or run on a busy
machine, to coax them out). Then **restore the patch**:

```sh
git checkout -- vendor/argtable3/argtable3.c && make
```

### Lab 4 (optional) — make the invisible visible with TSan

ThreadSanitizer instruments every memory access and reports races
*deterministically*, even when the wrong output never manifests. Add
`-fsanitize=thread -g` to the `CFLAGS` (and link flags) in
`shell/Makefile` and the `apps/*` Makefiles, rebuild, and rerun Lab 3's
loop with the patch stripped: TSan prints `WARNING: ThreadSanitizer:
data race` naming `optind` and both racing stacks — on the *first*
run, every run. With the patch restored: silence. Dynamic tools
couldn't see Chapter 3's un-executed OOM branch; TSan *can* see a race
that hasn't misbehaved yet — know which tool sees what.

---

## 5.7 Chapter 5 — the rules to keep

> **R28.** A thread = own note-pad (a stack in the mmap district), own
> registers, own TLS, own `errno` — and *nothing else*. Heap, globals,
> and the fd card-box are shared; a Speedrunner crash kills the room.
> ~10× cheaper than the Mirror, dangerous for the same reason.
>
> **R29.** Thread a pipeline by ownership: per stage, one heap packet,
> one `dup`'d card pair, two `fdopen`'d streams owned outright.
> `fclose(out)` is both flush and EOF — completion cascades down the
> belts as feed-end cards close. `pthread_join` all workers before
> touching anything they borrowed.
>
> **R30.** A data race = same box + ≥1 writer + no ordering = undefined
> behavior. Its signature is probabilistic failure: "it almost always
> works" is the *symptom*, not the refutation. Hunt with TSan, not
> with reruns.
>
> **R31.** Linkage is destiny: a library that keeps working state in
> globals (getopt's `optind`, `place`, permute indices) makes every
> caller single-threaded by default — transitively and silently. Audit
> what you link before you thread.
>
> **R32.** `__thread` (C11: `_Thread_local`) re-homes a global into a
> per-worker template (`.tdata`/`.tbss`), instantiated at thread
> birth, addressed via `%fs` in one instruction. Constant initializers
> only. `errno` walked this road first. It is the minimal cure when an
> API's *contract* is its globals — fix the visible externs **and**
> the hidden `static`s, including function-local ones.
>
> **R33.** The best lock is no shared data. Partition ownership first
> (per-call tables, per-stage streams, TLS); reach for `pthread_mutex`
> only when sharing is the *point*. mysh ships zero mutexes — by
> architecture, not by luck.

---

## ⏭ Next: CHAPTER 6 — *The Tin-Can Socket Timers*

The last cast member takes the stage: tin-can telephones strung
between rooms across the world, every one fitted with a five-second
self-destruct fuse — and the `@` that lets a line of plain English
skip the shell's grammar entirely and ride those cans to an AI.

---
---

# CHAPTER 6
# The Tin-Can Socket Timers: Non-Blocking TCP Sockets and the Grammatical AI Hook

> *In which every telephone can carries two labels on each end, a
> blocking `connect()` is revealed as an unkillable trap, two
> five-second fuses save the prompt — and one `@` at column zero swaps
> out the shell's entire grammar without a single fork.*

---

## 6.1 The tin-can telephone — sockets and the 4-tuple

A **socket** is the last new kind of card in this book: `socket()`
mints a card in your fd card-box (Chapter 2's table — it sits beside
files and pipes and obeys `close()` like any card), but the kernel
object behind it is a *telephone can* — an endpoint that can be tied
by a string to exactly one other can, anywhere on the internet.

What identifies a TCP connection? Not the fd (that's per-room), not
the port alone (a server talks to thousands of clients on one port).
**The connection is the 4-tuple** — both labels on both cans:

```
   your room (mysh)                              the AI mock server
   can: fd 7                                     can: fd 4
   ┌──────────────────────────┐                 ┌──────────────────────────┐
   │ 127.0.0.1 : 49152        │ ●━━━ string ━━▶ │ 127.0.0.1 : 5001         │
   │ (source IP) (source port │                 │ (dest IP)   (dest port — │
   │  — kernel-assigned, an   │                 │  you chose it: -p 5001)  │
   │  "ephemeral" port)       │                 │                          │
   └──────────────────────────┘                 └──────────────────────────┘

       the connection IS (127.0.0.1, 49152, 127.0.0.1, 5001) —
       every arriving TCP segment is routed to a can by matching
       ALL FOUR labels.  Same server, second client?  Different
       source port → different 4-tuple → different can.
```

As the client you choose only the far end (`-H HOST -p PORT`); the
kernel stamps your end automatically — source IP by routing, source
port from the ephemeral range (typically 32768–60999 on Linux). This
asymmetry is why clients are simple and Chapter 6 needs no `bind()`.

One more property before the lifecycle, because *everything* in
`fetch`'s code shape follows from it: **TCP is a byte stream, not a
message service.** The string between the cans carries bytes, in
order, reliably — and with *no boundaries*. `send()` may ship half
your bytes (queue full); `recv()` may hand you half a reply, or two
replies glued together. If you want messages, *you* draw the lines —
and your protocol does, with the oldest framing there is:

```c
    size_t  msglen   = strlen(the_msg);
    size_t  framelen = msglen + 1;               /* + '\n' */
```
*(`apps/fetch/cmd_fetch.c:320-321`)*

One exchange = one `'\n'`-terminated line each way. That single
design decision generates `send_all`'s loop, the `recv` loop's
`memchr`, and half of §6.2.

---

## 6.2 The Client Socket Lifecycle — five verbs, in order

`fetch_run` is the canonical TCP client, and your implementation walks
the canon in order: **resolve → socket → connect → send → recv →
close.** (With the bouncer at the door first: before any network
call, the host is length-checked against the RFC 1035 limit of 255,
swept for control characters, and the port range-checked 1–65535 —
`cmd_fetch.c:248-273`. Validate at the boundary; everything
downstream gets to trust its inputs.)

### Verb 1 — `getaddrinfo()`: the phone book

```c
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;   /* TCP */

    int rc = getaddrinfo(host, portstr, &hints, res);
    if (rc != 0) {
        *errmsg = gai_strerror(rc);
        return -1;
    }
```
*(`apps/fetch/cmd_fetch.c:82-92`, inside `resolve_host` — note the §1.8
taxonomy: two ins, two outs, error code in the return; and `res` is a
`struct addrinfo **`, an out-parameter returning a pointer)*

A name like `localhost` may map to several addresses (IPv6 `::1`
*and* IPv4 `127.0.0.1`); `getaddrinfo` returns them as a kernel-built
**linked list** — this book's first! — chained by `ai_next` Pips. The
`hints` struct narrows the listing: any family, but streams only.
(Two error-reporting worlds meet here: `getaddrinfo` fails with its
*own* codes via `gai_strerror`, not `errno` — your code keeps them
straight.)

### Verbs 2 + 3 — `socket()` and `connect()`, per candidate

```c
    int fd = -1;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;                            /* try next candidate */
        if (connect_timeout(fd, rp->ai_addr, rp->ai_addrlen,
                            FETCH_CONNECT_TIMEOUT_SECS) == 0)
            break;                               /* connected */
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
```
*(`apps/fetch/cmd_fetch.c:285-296`)*

Walk the Pip chain; for each candidate, mint a can (`socket`) and try
to tie the string (`connect_timeout` — §6.3's star). Failure closes
*that* can and tries the next address; success breaks out. Note the
ledger discipline on both paths: the failed can is closed before
moving on, and the phone book is returned (`freeaddrinfo`) the moment
the loop ends — R19 in its network habitat.

### Verb 4 — `send()`, looped into `send_all()`

```c
static int send_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}
```
*(`apps/fetch/cmd_fetch.c:149-161`)*

Because TCP is a stream (§6.1), `send` is allowed to take *some* of
your bytes and return — so honest code loops: `buf + off` (§1.4
pointer arithmetic) advances past what was taken until nothing
remains. The `EINTR` retry is the same armor as ever. Shape-match
this against Chapter 1's `write_all` in linedit (`linedit.c:129-140`):
the identical loop, one for a tty, one for a can — partial writes are
a fact of fds, not of networks.

### Verb 5 — `recv()`, looped until the line is whole

```c
    while (!saw_newline) {
        n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                FETCH_FAIL("recv", "timed out waiting for reply");
```
*(`apps/fetch/cmd_fetch.c:351-356`)*
```c
        if (n == 0)
            break;                               /* peer closed */
```
*(`:365-366`)*
```c
        if (memchr(buf, '\n', (size_t)n) != NULL)
            saw_newline = 1;                     /* full line received */
```
*(`:384-385`)*

Three ways a read ends, three different meanings: **bytes arrived**
(append them — into Chapter 3's third growable-buffer specimen, the
doubling `reply` accumulator at `:368-380`, now seen in its natural
habitat); **`n == 0`** — the peer hung up its can, the polite EOF
(§5.2's cascade, arriving from across the network); or **`EAGAIN`** —
the §6.3 fuse burned down. The loop exits when `memchr` spots the
`'\n'` frame mark: the *protocol*, not TCP, says the reply is
complete. Then `close(fd)` (`:387`) hangs up, and every exit door on
the way settled the full triple — `free(reply)`, `close(fd)`,
`arg_freetable` — exactly as audited in §3.5.

---

## 6.3 The unkillable hang — and the two five-second fuses

Here is why this chapter's character carries *timers*, not just cans.

**A plain blocking `connect()` can park a process for minutes.** Send
a SYN to a routable address where a firewall silently DROPs packets
(no RST, no ICMP — just void), and the kernel dutifully retransmits:
six retries, exponentially spaced — **about two minutes** on stock
Linux before it gives up. A blocking `recv()` on a peer that
connected but never answers can wait *forever*.

For most programs that's an annoyance. For yours it is a trap with no
exit, and two of your own design decisions spring it:

1. `fetch` runs **in-process** (§2.4, §4.3) — on the shell's main
   thread. While it waits, there is no prompt.
2. The shell **ignores SIGINT** (`signal(SIGINT, SIG_IGN)`,
   `mysh.c:553`) — and with disposition `SIG_IGN`, Ctrl-C doesn't even
   interrupt the blocked syscall. The user's panic key does *nothing*.

An unbounded network wait in mysh is therefore an **unkillable frozen
shell** (short of `kill -9` from another terminal). The Tin-Can
Timers are not politeness; they are survival. And the bound they buy
is concrete: connect ≤ 5 s, reply-silence ≤ 5 s — the worst case `@`
can cost the prompt is about ten seconds, ever:

```c
#define FETCH_CONNECT_TIMEOUT_SECS 5
#define FETCH_RECV_TIMEOUT_SECS    5
```
*(`apps/fetch/cmd_fetch.c:14-15`)*

### Fuse 1 — the non-blocking connect, line by line

```c
static int connect_timeout(int fd, const struct sockaddr *addr,
                           socklen_t alen, int timeout_secs)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    int rc = connect(fd, addr, alen);
    if (rc == 0) {
        /* Connected immediately (e.g. loopback). */
        fcntl(fd, F_SETFL, flags);
        return 0;
    }
    if (errno != EINPROGRESS)
        return -1;                       /* hard failure, errno already set */
```
*(`apps/fetch/cmd_fetch.c:100-116`)*

Move by move:

- **`F_GETFL` then `F_SETFL … | O_NONBLOCK`** — read the can's current
  mode flags, set the *don't-wait* bit. (Read-modify-write, never
  blind-write: other flags survive. And note the symmetry coming —
  what gets toggled gets restored.)
- **`connect()` in non-blocking mode doesn't wait** — it *launches*
  the handshake and returns immediately. Two answers are possible:
  `0` (already connected — loopback handshakes complete instantly,
  which is why your `@`-to-localhost usually takes this early exit),
  or `-1` with **`errno == EINPROGRESS`** — not an error, a *receipt*:
  "the SYN is in flight; check back." Anything else is a real failure.

Now the wait — bounded, and aimed at the right event:

```c
    /* Wait for the socket to become writable (connect complete) or time out. */
    struct pollfd pfd = { .fd = fd, .events = POLLOUT };
    int pr;
    do {
        pr = poll(&pfd, 1, timeout_secs * 1000);
    } while (pr < 0 && errno == EINTR);

    if (pr == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
```
*(`apps/fetch/cmd_fetch.c:118-128`)*

`poll` is the kernel's *"wake me when something happens to these fds —
or when the fuse burns out."* You hand it an array of watched cans
(here: one), the events you care about (a connecting socket becomes
**writable** when the handshake resolves — hence `POLLOUT`), and the
fuse length in milliseconds. Three outcomes: `>0` something happened;
`0` **the fuse** — your code converts that into honest `errno =
ETIMEDOUT`; `<0` error (with the EINTR retry, and note the loop
re-arms a full timeout — acceptable armor here since no handlers are
installed). You have seen this exact tool before at a different
timescale: linedit's escape-sequence decoder polls the keyboard with
a **50 ms** fuse (`read_byte_timeout`, `shell/linedit.c:118-127`) to
tell a lone ESC from an arrow key. Same `struct pollfd`, same
semantics — 50 ms for fingers, 5000 ms for continents. And the same
call generalizes to *N* cans watched at once — that's multiplexing,
the heart of every server and event loop; your two uses are its
single-fd special case.

Then the expert beat — the line that separates working code from
folklore:

```c
    /* poll() reports writability even on failed connects; check SO_ERROR. */
    int soerr = 0;
    socklen_t len = sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0)
        return -1;
    if (soerr != 0) {
        errno = soerr;
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags) < 0)   /* restore blocking mode */
        return -1;
    return 0;
}
```
*(`apps/fetch/cmd_fetch.c:132-145`)*

**Writable does not mean connected.** A *refused* connection also
"resolves" the handshake — `poll` wakes up either way. The verdict is
parked in the socket's error slot, read (and cleared) by
`getsockopt(SO_ERROR)`: zero means tied, nonzero is the real `errno`
(`ECONNREFUSED`, `EHOSTUNREACH`, …), which your code faithfully
republishes. Skip this check and refused connections masquerade as
successes until the first `send` mysteriously dies. Finally the
toggled flag is restored — the can goes back to blocking mode, so the
rest of the lifecycle (`send`/`recv`) stays simple.

### Fuse 2 — `SO_RCVTIMEO`, for the line that connects and then goes quiet

```c
    struct timeval tv;
    tv.tv_sec  = FETCH_RECV_TIMEOUT_SECS;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
```
*(`apps/fetch/cmd_fetch.c:307-310`)*

A different failure, a different tool: the server *answered the
phone* and then said nothing. `SO_RCVTIMEO` arms the can itself —
every subsequent `recv()` fails with `EAGAIN`/`EWOULDBLOCK` after 5
silent seconds, which §6.2's loop translates into *"timed out waiting
for reply."* No poll needed; the fuse rides the socket.

Three failure timescales, three mechanisms — keep the taxonomy:

| Failure | Feels like | Caught by | Typical verdict |
|---|---|---|---|
| nobody picks up (DROP) | infinite dial tone | `poll` fuse on connect | `ETIMEDOUT` after 5 s |
| picks up, never speaks | open line, silence | `SO_RCVTIMEO` on recv | "timed out waiting for reply" after 5 s |
| hangs up mid-call | click | `recv() == 0` | partial reply, loop ends |

(And when things fail with `--json`, `FETCH_FAIL` emits a structured
`{"ok": false, …}` object instead of prose — `cmd_fetch.c:237-246` —
your course's agent-integration contract, honored even in defeat.)

---

## 6.4 The `@` Grammar Hook — a second language, chosen by one byte

Now the integration that makes this the capstone chapter. Your shell
speaks two languages at the prompt: the pipeline grammar (Chapter 7's
tokens, quotes, `|`, `>`), and **plain English prefixed by `@`** —
forwarded to an AI endpoint over the cans we just built. The deep
design insight is *where* the language is chosen: **before
tokenization, on the first non-blank byte.**

Why must it be that early? Because natural language would be mangled
by the command grammar: in *"@ what's in notes.txt | sorted?"*, the
apostrophe would open a quote, the `|` would split a pipeline — prose
isn't tokens. So the hook intercepts the raw line, ahead of
`tok_split`, exactly as the comment promises:

```c
        /* ── natural-language '@' prefix ────────────────────────────────
         * If the first non-whitespace character is '@', bypass tokenization
         * and pipeline parsing entirely.  The remainder of the line is a raw
         * prompt forwarded to the AI mock server via fetch. */
        {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;   /* first non-whitespace */
            if (*p == '@') {
                p++;                                   /* skip '@' */
                while (*p == ' ' || *p == '\t') p++;   /* skip leading spaces */
```
*(`shell/mysh.c:618-627`)*

Note the craft: `p` slides along `main`'s own `line[]` buffer — no
copies, just §1.4 pointer arithmetic — and the prefix scan tolerates
leading whitespace, so `  @ help` works. Then three guard clauses,
each a design decision:

**Guard 1 — interactive only:**

```c
                if (!interactive) {
                    /* '@' is an interactive convenience; ignore it in scripts
                     * and piped input so non-interactive runs stay offline
                     * and deterministic. */
```
*(`shell/mysh.c:629-632`)*

A script that secretly phones an AI is a script whose output depends
on the network and a model's mood — your shell refuses on principle.
Batch runs stay **offline and deterministic** (and your test suite
stays meaningful). Compare Chapter 2's `g_cmd_fd` sidebar: once
again mysh chooses the reproducibility contract.

**Guard 2 — strip one optional pair of quotes** (`:639-645`): a user
who types `@ "what is 2+2"` out of shell habit shouldn't have literal
quotes sent to the model — but only *one surrounding pair* is
removed; interior quotes are prose and survive.

**Guard 3 — empty prompt** (`:648-651`): `@` alone is an error, status
1, no network call.

Then the hand-off — and you have already studied every technique in
it:

```c
/*
 * Forward a natural-language prompt to the AI mock endpoint by reusing the
 * fetch command's logic in-process (cmd_fetch_spec.run) — no fork, no second
 * parse.  The prompt becomes fetch's MESSAGE positional; the "--" guards
 * prompts that happen to start with '-'.  fetch writes the server's reply to
 * out_stream itself, so the suggestion lands cleanly on the user's stream.
 * Returns fetch's exit code.
 */
static int run_at_prompt(const char *prompt, FILE *in, FILE *out)
{
    char *fargv[] = {
        "fetch", "-H", AI_HOST, "-p", AI_PORT, "--",
        (char *)prompt, NULL
    };
    int fargc = (int)(sizeof fargv / sizeof fargv[0]) - 1;
    return cmd_fetch_spec.run(fargc, fargv, in, out);
}
```
*(`shell/mysh.c:511-527`)*

Unpack it Pip by Pip:

- **A synthetic argv, hand-built on the tray** — §1.3's NULL-fenced
  shelf, constructed in eight lines instead of by a parser. `fargc`
  is computed by the classic idiom *count the slots, subtract the
  fence*: `sizeof fargv / sizeof fargv[0] - 1` → 7.
- **The `"--"` sentinel** — Chapter 5's getopt engine treats `--` as
  *end of options*: everything after is positional, never a flag. So
  a prompt like *"-rn flags, what do they mean?"* can't be
  misparsed as options. One array slot buys total safety.
- **`(char *)prompt`** — argv cells are historically `char *`; the
  cast shed a `const` to fit the 50-year-old shape (and `fetch` never
  writes through it).
- **`cmd_fetch_spec.run(fargc, fargv, in, out)`** — §4.3's indirect
  call, invoked *directly on the struct* this time: no registry scan
  needed, the shell knows exactly which machine it wants. **No fork.
  No second parse** (the line was never tokenized; argtable3 parses
  the synthetic argv once, inside `fetch_run`). One process, one
  thread, one function call into a TCP client.

The whole journey, end to end — every chapter of this book in one
keystroke's trace:

```
 mysh> @ how do I count lines?
   │  linedit_read → line[]                      (§1.6 — the bounded buffer)
   │  first non-blank byte == '@' → grammar switch (no tok_split, no stages)
   │  run_at_prompt("how do I count lines?", input, stdout)
   │    fargv = {"fetch","-H","localhost","-p","5001","--",prompt,NULL}
   │    cmd_fetch_spec.run(7, fargv, in, out)    (§4.3 — the machine Pip)
   │       validate → getaddrinfo → socket → connect_timeout (fuse 1)
   │       → send_all("how do I count lines?\n") → recv …'\n' (fuse 2)
   │       → reply written to out_stream → ledgers settled    (§6.2–6.3)
   ▼
 wc -l notes.txt          ← the server's suggestion, on your tty
 mysh>                    ← the prompt survives — worst case, ten seconds
```

This is what "system integration" means in this book: the AI hook is
not a subsystem. It is the anatomy (Ch 4) composing the network
client (Ch 6) over the in-process bet (Ch 2) with tray-built argv
(Ch 1), bounded by timers so the room (Ch 5's workers included) never
hangs. Remove any one chapter and this feature doesn't work; with all
of them, it's nineteen lines.

---

## 6.5 Lab — be the AI, burn the fuses

### Lab 1 — answer your own shell

Terminal A — play the AI server:
```sh
nc -l 5001          # some netcats spell it: nc -l -p 5001
```
Terminal B:
```sh
./shell/mysh
mysh> @ what is 2+2
```
Terminal A now shows `what is 2+2` — your prompt, framed with its
newline, fresh off the can. Type an answer and press Enter:
```
try: echo 4
```
…and it appears in mysh as the reply, prompt restored. You have
hand-played one full line-protocol exchange: §6.2's five verbs, with
you as `recv`'s far end.

### Lab 2 — photograph the 4-tuple

While Lab 1's connection is waiting for your reply (the recv fuse
gives you 5 seconds — type fast or just re-run):
```sh
ss -tnp | grep 5001
```
Two lines — one per direction — showing
`127.0.0.1:<ephemeral> ↔ 127.0.0.1:5001`: the 4-tuple of §6.1,
photographed live, with your shell's PID attached to one end and
`nc`'s to the other.

### Lab 3 — all three failure timescales

```sh
# 1. Refused: nothing listens — handshake resolves instantly, SO_ERROR speaks
printf 'fetch -H localhost -p 1 hi\nexit\n' | ./shell/mysh

# 2. Connect fuse: a documentation-reserved address that routes nowhere
time ( printf 'fetch -H 192.0.2.1 -p 9 hi\nexit\n' | ./shell/mysh )

# 3. Recv fuse: a server that answers and says nothing
nc -l 5001 &        # answer the phone, stay silent
printf 'fetch -H localhost -p 5001 hi\nexit\n' | ./shell/mysh
kill %1
```

Expect: (1) `Connection refused`, immediately — that's `poll` waking
on a *failed* handshake and `getsockopt(SO_ERROR)` reading the
verdict; (2) ~5.0 s then `Connection timed out` — fuse 1 (on some
networks a router sends ICMP instead and you'll get a fast `No route
to host`; both are *bounded* — the unbounded case is the silent DROP
this fuse exists for); (3) ~5 s then `timed out waiting for reply` —
fuse 2. Three failures, three timescales, all survivable.

### Lab 4 — the lifecycle as a syscall transcript

With Lab 1's `nc` listening:
```sh
printf 'fetch -H localhost -p 5001 hello\nexit\n' | \
  strace -f -e trace=socket,connect,poll,getsockopt,setsockopt,sendto,recvfrom,close \
  ./shell/mysh 2>&1 | grep -E 'socket|connect|poll|sockopt|sendto|recvfrom' | head
```
Read §6.2 back out of the kernel's mouth: `socket(AF_INET…)`,
`connect(… EINPROGRESS or 0)`, possibly `poll([{fd, POLLOUT}], 1,
5000)`, `getsockopt(SO_ERROR → 0)`, `setsockopt(SO_RCVTIMEO)`,
`sendto("hello\n")`, `recvfrom(…)`, `close`. (Reply in the `nc`
terminal quickly, or the transcript ends with the recv fuse — equally
instructive.) On loopback, `connect` often returns 0 immediately —
the early exit at `cmd_fetch.c:111-115`, caught in the act.

---

## 6.6 Chapter 6 — the rules to keep

> **R34.** A socket is a card to a telephone can; a TCP connection's
> identity is the **4-tuple** (src IP, src port, dst IP, dst port).
> Clients choose the far labels; the kernel stamps the near ones
> (ephemeral port). `getaddrinfo` turns one name into a candidate Pip
> chain — walk it with per-candidate `socket`/`connect`/`close`, and
> `freeaddrinfo` when done.
>
> **R35.** TCP is a byte **stream**: `send` may be partial (loop —
> `send_all`), `recv` chunks arbitrarily (loop until *your* frame
> mark — `memchr('\n')` — or `n == 0`, the polite hangup). Message
> boundaries are the application's job; one trailing `'\n'` is a
> complete protocol.
>
> **R36.** Never wait unbounded — in your shell it's existential:
> in-process commands + an SIGINT-ignoring shell make a blocking
> `connect` (≈2 min of kernel SYN retries) an *unkillable frozen
> prompt*. Fuse every wait: `O_NONBLOCK` + `EINPROGRESS` + `poll(…,
> timeout)` for connect; `SO_RCVTIMEO` for silence; and after `poll`
> says writable, **read the verdict from `getsockopt(SO_ERROR)`** —
> writability is not success.
>
> **R37.** Restore what you toggle (`F_GETFL` → `| O_NONBLOCK` → put
> it back), and settle every door: free the frame, free the reply,
> close the can, free the tables — R19, now with sockets in it.
>
> **R38.** `poll` is one tool at every timescale — 50 ms to tell ESC
> from an arrow key (`linedit.c:118`), 5000 ms to tell a slow server
> from a dead one (`cmd_fetch.c:122`) — and it scales to N cans at
> once: that's multiplexing, and you've already written its
> single-can special case twice.
>
> **R39.** Choose grammars by the first byte, *before* tokenizing
> (`@` = prose, `#` = comment) — prose must never meet the pipeline
> parser. Deliver the escape hatch by composition: a tray-built,
> NULL-fenced argv, a `"--"` end-of-options sentinel, and one direct
> `spec.run(…)` call — no fork, no second parse, and scripts stay
> offline and deterministic.

---

## ⏭ Next: CHAPTER 7 — *Strings, Tokens, and the Pipeline Parser State Machine*

Between the keyboard and the registry stands the parser — the hops
§4.3 fast-forwarded through and §6.4 deliberately bypassed. Pip
returns with a knife: two schools of string slicing, a three-mode
lexer, `$?` injected mid-word — and an audit that finds a sentence
which crashes the entire shell.

---
---

# CHAPTER 7
# Strings, Tokens, and the Pipeline Parser State Machine

> *In which Pip slices a row of bytes two different ways, one scanning
> loop lives in three states, `$?` becomes `"127"` in mid-word — and
> reading the grammar against the code turns up a typo-sized sentence
> that kills the whole shell. (Verified. Fixed in Lab 5.)*

---

## 7.1 Pip the string-slicer — two schools of cutting

A C string, Chapter 1 taught, is a row of byte-boxes ending at a `'\0'`
fence. To *slice* one — to carve `"sort -rn"` into `sort` and `-rn` —
your codebase practices two distinct schools, and knowing which one
you're in is the difference between elegance and vandalism.

**School 1: drop a fence in place.** Pip walks to the cut point and
writes `'\0'` right into the row. Instant, zero allocation — and
*destructive*: the original string is now permanently (or temporarily)
two strings. Your shell does this three times, each deliberate:

```c
        line[strcspn(line, "\n")] = '\0';
```
*(`shell/mysh.c:616` — chop the newline: find its index, fence it off)*

```c
                char *eq = strchr(stages[0].argv[0], '=');
                if (eq && eq > stages[0].argv[0] && stages[0].argc == 1) {
                    *eq = '\0';
```
*(`shell/mysh.c:689-691` — split `FOO=bar` at the `=` for `setenv`)*
```c
                    *eq = '=';  /* restore if not a valid assignment */
```
*(`shell/mysh.c:701` — and if the name turns out invalid, **the wall
goes back up**. In-place slicing is reversible vandalism — your code
keeps the receipt.)*

(The third specimen was §6.4's quote-stripper, fencing off a trailing
`"` in place.)

**School 2: measure and duplicate.** Pip marks a start and a length,
then rents a fresh heap box and copies the slice out, fence included.
The original survives untouched. This is the lexer's school — and
`tok.c` spells the technique out by hand:

```c
static int push_word(tok_t *t, const char *buf, int len)
{
    if (t->n >= TOK_MAX) {
        fprintf(stderr, "mysh: too many tokens on one line\n");
        return -1;
    }
    char *s = malloc((size_t)len + 1);
    if (!s) { perror("malloc"); return -1; }
    memcpy(s, buf, (size_t)len);
    s[len] = '\0';
    t->w[t->n++] = s;
    return 0;
}
```
*(`shell/tok.c:62-74`)*

`malloc(len + 1)`, copy `len` bytes, plant the fence — this is
**`strndup`, hand-rolled** (the libc function your `sort` uses at
`cmd_sort.c:135` does exactly these four lines). Why must the lexer
duplicate rather than fence-drop into `line[]`? Lifetime: the tokens
out-live the scan — they are borrowed by `stage_t` buckets (§7.4),
ride Speedrunners for the whole pipeline (§5.2's lease), and only die
at `tok_free`:

```c
void tok_free(tok_t *t)
{
    for (int i = 0; i < t->n; i++) { free(t->w[i]); t->w[i] = NULL; }
    t->n = 0;
}
```
*(`shell/tok.c:169-173` — contents freed, Pips NULLed — R20's
free-then-NULL, practiced even here)*

> **The rule of thumb:** fence-drop when the original may be consumed
> (and restore if you guessed wrong); duplicate when the slice must
> outlive the scan. Lexers duplicate.

---

## 7.2 One loop, three modes — the quote-aware state machine

Here is the machine's whole working set — a scratch row, a length, and
three booleans:

```c
int tok_split(const char *line, tok_t *out, int last_status)
{
    out->n = 0;

    char buf[WORDBUF];
    int  blen   = 0;
    int  in_sq  = 0;   /* inside '...' */
    int  in_dq  = 0;   /* inside "..." */
    int  in_word = 0;  /* building a word (matters for empty-quote handling) */

    for (const char *p = line; ; p++) {
```
*(`shell/tok.c:76-86`; `WORDBUF` is 4096)*

One Pip — `p` — walks the line byte by byte; characters accumulate in
`buf` until a word ends, then `push_word` duplicates the slice. What
makes it a **state machine** is that the *same byte means different
things depending on which mode the loop is in*:

```
                 '…' opens   ┌─────────────────────────────────────────┐
              ┌─────────────▶│ IN_SINGLE — absolute literalism:        │
              │              │ every byte verbatim ($, ", \, |, space  │
              │              │ are just bytes).  Only ' exits.         │──┐
              │              └─────────────────────────────────────────┘  │ ' closes
   ┌──────────┴──┐                                                        ▼
   │   NORMAL    │◀────────────────────────────────────────────────────────
   │ space=split │
   │ #=comment*  │  "…" opens ┌─────────────────────────────────────────┐
   │ |<>>&=ops   │───────────▶│ IN_DOUBLE — almost literal: space and   │
   │ $=expand    │            │ operators are plain bytes; \" \\ \$     │
   └─────────────┘◀───────────│ escape; $ expands (§7.3). Only " exits. │
                   " closes   └─────────────────────────────────────────┘
                                      (*# comments only at word start)
```

### Single quotes: absolute isolation

```c
        if (in_sq) {
            if (c == '\0') {
                fprintf(stderr, "mysh: unmatched single quote\n");
                tok_free(out);
                return -1;
            }
            if (c == '\'') { in_sq = 0; continue; }
            if (blen < WORDBUF - 1) buf[blen++] = c;
            continue;
        }
```
*(`shell/tok.c:90-99`)*

Three lines of policy: end-of-line inside a quote is an error (note
the door discipline — `tok_free` settles the already-pushed words
before the `-1`, R19 even mid-lexer); a `'` exits the mode; *anything
else is a byte*. No escapes exist here — `'\$?'` would hold a real
backslash. Maximum predictability, zero expressiveness: the bunker.

### Double quotes: literal, with three doors and one live wire

```c
        if (in_dq) {
```
```c
            if (c == '"')  { in_dq = 0; continue; }
            if (c == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$')) {
                p++;
                c = *p;
                /* fall through: write escaped char literally */
            } else if (c == '$') {
                do_expand(&p, buf, &blen, last_status);
                continue;
            }
            if (blen < WORDBUF - 1) buf[blen++] = c;
            continue;
        }
```
*(`shell/tok.c:102-118`, trimmed)*

Spaces, pipes, semicolons — all plain bytes in here, which is
precisely **why `"hello world"` is one token**: the space at index 6
arrives while `in_dq` is set, falls through to `buf[blen++] = c`, and
never reaches the normal-mode splitting logic. Exactly three escapes
are honored (`\"`, `\\`, `\$` — the one-byte lookahead `p[1]` decides),
and the `$` is a **live wire**: expansion runs inside double quotes
(§7.3). So the three modes give you a spectrum: `'$?'` is two literal
bytes, `"$?"` is the exit status, bare `$?` is the exit status *and*
subject to word-splitting at spaces around it.

*(A documentation note, in the spirit of your BNF handout's own advice
— "treat AI output as untrusted until you can build and verify": the
terse comment in `tok.h:13` says quoting has "no variable expansion."
The code disagrees — double quotes expand. We verified the code's
version live: `echo "$?"` prints a number, not `$?`. The header
undersells its own lexer; the source is the truth.)*

### Normal mode: where words end and operators are born

```c
        if (c == '\0' || isspace((unsigned char)c)) {
            /* end of word */
            if (in_word || blen > 0) {
                if (push_word(out, buf, blen) < 0) { tok_free(out); return -1; }
                blen = 0;
                in_word = 0;
            }
            if (c == '\0') break;
            continue;
        }

        if (c == '#' && !in_word) break;   /* comment */

        if (c == '\'') { in_sq = 1; in_word = 1; continue; }
        if (c == '"')  { in_dq = 1; in_word = 1; continue; }
```
*(`shell/tok.c:123-137`)*

Four subtleties hiding in plain sight:

1. **Quotes toggle modes; they never split words.** Entering or
   leaving quote mode doesn't `push_word` — so adjacent pieces *glue*:
   `ab'cd'ef` lexes to one token `abcdef` (verified). Quoting controls
   *interpretation*, not *boundaries*.
2. **The `in_word` flag exists for empty quotes.** `""` sets `in_word`
   without adding bytes; at the next space, `in_word || blen > 0`
   pushes a genuine **empty token** — so `wc ""` passes an empty
   filename and reports `wc: : No such file or directory` (verified).
   Without the flag, `""` would simply vanish.
3. **`#` comments only at a word boundary** (`!in_word`): `echo a#b`
   prints `a#b`, while `echo hi #comment` prints `hi` (both verified)
   — the same rule bash follows.
4. **Overflow policy: drop, don't breach.** Every append is guarded by
   `blen < WORDBUF - 1`; byte 4096 of a monster word is silently
   discarded — the same "line full: drop the key" policy as linedit's
   `le_insert` (`linedit.c:200`). Never trade correctness for input.

And operators:

```c
        if (c == '>' || c == '<' || c == '|' || c == '&') {
            /* flush any pending word first */
```
```c
            if (c == '>' && p[1] == '>') {
                if (push_word(out, ">>", 2) < 0) { tok_free(out); return -1; }
                p++;
            } else {
                if (push_word(out, (char[]){c, '\0'}, 1) < 0) { tok_free(out); return -1; }
            }
```
*(`shell/tok.c:145-157`, trimmed)*

An operator both **ends the current word** (the flush) and **is its
own token** — `ls>out` needs no spaces. The one-byte lookahead `p[1]`
welds `>>` into a single token before `>` can claim it. And savor
`(char[]){c, '\0'}` — a *compound literal*, C99's anonymous stack
array built mid-expression: a two-byte row conjured just long enough
for `push_word` to duplicate it. Pip pointing at a box with no name.

---

## 7.3 Runtime variable injection — `do_expand`

The `$` branch hands the scanning cursor itself to a helper — read the
contract first, it's the subtlest pointer hand-off in the repo:

```c
/* Expand $VAR / ${VAR} / $? at position *pp (which points to '$').
 * Appends the expanded text into buf[0..WORDBUF-1] via *blen.
 * On return *pp points to the last consumed character so the caller's
 * loop p++ lands on the first unprocessed character. */
static void do_expand(const char **pp, char *buf, int *blen, int last_status)
```
*(`shell/tok.c:12-16`)*

`const char **pp` — Pip-on-Pip, §1.6's out-parameter, but this time
the "thing being returned" is **the caller's bookmark**: `do_expand`
walks ahead, consumes the variable reference, and parks `*pp` exactly
one byte *short*, so the caller's `p++` lands cleanly on the next
unprocessed character. (Compare Chapter 5: getopt kept its scanning
cursor `place` in a *global* and needed `__thread` surgery; `tok.c`
threads its cursor *explicitly through parameters* — same problem,
the architectural answer, no TLS required. Signatures beat globals.)

Four reference shapes, four branches:

```c
    if (*p == '?') {
        snprintf(numbuf, sizeof numbuf, "%d", last_status);
        val = numbuf;
```
*(`shell/tok.c:26-28` — `$?`: the Chapter 2 status circuit's **final
leg**. `last_status` rode `waitpid`→`WEXITSTATUS`→`main`, was passed
into `tok_split(line, &tok, last_status)` at `mysh.c:662`, and here
becomes ASCII digits in mid-word.)*

```c
    } else if (*p == '{') {
        p++;  /* skip '{' */
        while (*p && *p != '}') {
```
*(`:30-32` — `${VAR}`: braces delimit explicitly; note the guard at
`:37` — an unterminated `${` backs up rather than marching past the
fence.)*

```c
    } else if (isalpha((unsigned char)*p) || *p == '_') {
        while (isalnum((unsigned char)*p) || *p == '_') {
```
*(`:40-41` — bare `$VAR`: identifier rules, letter-or-underscore then
alphanumerics — `$HOME/file` knows to stop at the `/`.)*

```c
    } else {
        /* bare '$': emit literally, back up so current char is re-examined */
```
*(`:48-49` — a lone `$` (e.g. `$ 5.00`) is just a dollar sign.)*

Then the unified ending: look the name up (`getenv`; `$?` already has
`val`), and append whatever was found into the **current word's**
scratch buffer:

```c
    if (val) {
        for (const char *v = val; *v && *blen < WORDBUF - 1; v++)
            buf[(*blen)++] = *v;
    }
```
*(`shell/tok.c:54-57` — unset variable ⇒ `val == NULL` ⇒ append
nothing: `$NOPE` expands to empty, silently.)*

That little append loop carries this chapter's biggest design fact:
**expansion is injection, not re-tokenization.** The expanded bytes
land in `buf` and are *never rescanned* — the scanning loop walks the
original `line`, not the buffer. Consequence, verified live:

```
mysh> FOO='a b'
mysh> wc $FOO
wc: a b: No such file or directory
```

One token. `wc` received a single filename containing a space. POSIX
shells would re-split `$FOO` into two words (the `$IFS` dance that
forces everyone to write `"$FOO"` defensively); mysh's expansion is
structurally immune — a variable's value can never explode into extra
arguments or, worse, extra *pipeline stages*. Your lexer is safer
than `sh` by construction, and it got there by doing *less*.

---

## 7.4 The pipeline bucket parser — tokens into `stage_t`

The lexer's output is a flat shelf of words. The parser's job is
*shape*: walk the shelf once, dealing words into **buckets** — one
`stage_t` per pipeline stage:

```c
typedef struct {
    char *argv[TOK_MAX];
    int   argc;
    char *redir_in;    /* filename for <,  or NULL */
    char *redir_out;   /* filename for > or >>,  or NULL */
    int   append;      /* 1 if >> */
} stage_t;
```
*(`shell/mysh.c:98-104`)*

For `sort < in.txt | uniq -c > out.txt`, the dealing looks like:

```
 tokens: ["sort"] ["<"] ["in.txt"] ["|"] ["uniq"] ["-c"] [">"] ["out.txt"]
             │      └────┬────┘      │      │       │     └────┬─────┘
             ▼           ▼           ▼      ▼       ▼          ▼
        ┌─ stage[0] ────────────┐  seal & ┌─ stage[1] ──────────────┐
        │ argv  {"sort", NULL}  │  next   │ argv {"uniq","-c",NULL} │
        │ redir_in  ●─▶"in.txt" │  bucket │ redir_out ●─▶"out.txt"  │
        │ redir_out NULL        │         │ append 0                │
        └───────────────────────┘         └─────────────────────────┘
        every Pip in every bucket is BORROWED from tok_t's heap words
        — buckets are views, freed by nobody, valid until tok_free
```

The walk itself is one loop with four cases (`shell/mysh.c:108-155`):

**Case `|` — seal and advance:**

```c
        if (strcmp(w, "|") == 0) {
            cur->argv[cur->argc] = NULL;
            if (*nstages >= PIPELINE_MAX) {
                fprintf(stderr, "mysh: too many pipe stages\n");
                return -1;
            }
            cur = &stages[(*nstages)++];
            memset(cur, 0, sizeof *cur);
```
*(`mysh.c:120-127` — plant the argv NULL fence (§1.3's load-bearing
contract!), bounds-check, move `cur` to a zeroed fresh bucket —
`sizeof *cur`, the §1.8 self-maintaining idiom.)*

**Cases `<`, `>`, `>>` — steal the next token into a field:**

```c
        } else if (strcmp(w, "<") == 0) {
            if (++i >= tok->n) { fprintf(stderr, "mysh: expected filename after '<'\n"); return -1; }
            cur->redir_in = tok->w[i];
```
*(`mysh.c:129-131`; `>` and `>>` mirror it at `:133-141`, with `>>`
also setting `append = 1` — which Chapter 8 will turn into
`O_APPEND` vs `O_TRUNC`.)*

The `++i` is the move to notice: the operator **consumes its
neighbor**. The filename never reaches `argv` — it's routed to a
*field*. That's why `wc` never sees `out.txt` in a redirected
pipeline: by the time `spec->run(argc, argv, …)` fires (§4.3),
redirection has been compiled out of the argument vector entirely.
(Droll side effect of field-routing: order within a stage is free, so
`> out.txt ls -l` parses fine — the bucket ends up identical.)

**Default — deal the word:**

```c
        } else {
            if (cur->argc >= TOK_MAX - 1) {
                fprintf(stderr, "mysh: too many arguments\n");
                return -1;
            }
            cur->argv[cur->argc++] = w;
        }
```
*(`mysh.c:143-149` — note `TOK_MAX - 1`: one slot is always reserved
for the fence.)*

And the **layering payoff**, the reason shells have lexers at all: a
`|` typed inside quotes became an ordinary buffered byte back in §7.2
— it was *never pushed as an operator token*. So the parser may trust
every `"|"` it sees, unconditionally. Quoting is resolved exactly
once, at the character level; structure is resolved exactly once, at
the token level; execution happens at the bucket level:

```
   characters ──tok.c──▶ tokens ──parse_pipeline──▶ buckets ──run_pipeline──▶
   (quotes, $, \)        (words, ops)               (stage_t)    threads & forks
        §7.2–7.3              §7.4                      §2, §5
```

Your repo's BNF lab handout (`bnf_shell_lab.md`) asks students to
build this same three-box pipeline with a generated parser (BNFC or
Flex/Bison: *"grammar → generated parser → execution integration"*).
`tok.c` + `parse_pipeline` are the hand-rolled equivalent — about 220
lines total, the right call at this grammar's size; the generators
win when grammars grow operators, precedence, `&&`/`||`, subshells.

---

## 7.5 The grammar on paper — and the sentence that kills the shell

Write down what the code accepts — mysh's effective grammar, with
each production's enforcement point:

```
line      ::= "@" prose                      (mysh.c:618 — before lexing, Ch 6)
            | "#" comment                    (tok.c:134 — at word start)
            | pipeline [ "&" ]               ("&" only line-final: mysh.c:667)
            | ε
pipeline  ::= stage { "|" stage }            (parse_pipeline)
stage     ::= { word | redirect }            (≥ 1 word … or so we assume)
redirect  ::= "<" word | ">" word | ">>" word
word      ::= { char | "'" raw "'" | '"' cooked '"' | "$" expansion }
```

The handout's last deliverable is *"a small test suite with valid and
**invalid** syntax cases."* So let's audit the invalid sentences —
what happens when `stage` gets **zero words**?

- `| ls` — first token is `|`: stage 0 seals empty, and the guard at
  `mysh.c:153` (`if (stages[0].argc == 0) { *nstages = 0; … }`)
  quietly discards the whole line. Silently ignored. Odd, but safe.
- `ls |` — trailing pipe. The `|` branch happily opens a fresh bucket;
  tokens run out; the bucket is sealed with `argc == 0` and
  `argv[0] == NULL`. The guard only inspects `stages[0]` — **this
  empty bucket ships to the executor.** Trace it forward:
  `run_pipeline` → `reg_find(s->argv[0])` (`mysh.c:285`) →
  `strcmp(registry[i]->name, NULL)` → a walk into the crater.

Verified, not theorized:

```sh
$ printf 'ls |\necho survived\n' | ./shell/mysh
Segmentation fault (core dumped)        # exit status 139; nothing survived
$ printf 'echo a | | wc -l\n' | ./shell/mysh
Segmentation fault (core dumped)        # same hole, mid-pipeline
```

One typo'd byte at an interactive prompt — a trailing `|` — and the
**entire shell dies**, because (R13, Chapter 2) there is no Mirror
clone to absorb the crash: the parser, the registry, and your session
all live in one room. bash answers a trailing `|` with a continuation
prompt; mysh answers with `SIGSEGV`. A grammar hole in an in-process
shell is a *crash* hole.

This is the chapter's real lesson about parsers: **a parser earns
trust through its invalid sentences, not its valid ones.** Every
production in your BNF that says "at least one word" is a claim the
code must *enforce*, not assume. Lab 5 closes the hole — it is not
optional homework.

---

## 7.6 Lab — probe the lexer, then fix the parser

### Lab 1 — the quote triple (one line, one status)

```
mysh> nosuchx
mysh: nosuchx: No such file or directory
mysh [127]> echo $? "$?" '$?'
127 127 $?
```

All three referencing styles in a single command, so `last_status` is
sampled once: bare `$?` expands; `"$?"` expands (the live wire of
§7.2); `'$?'` is two bytes in the bunker. *(Why one line? Each
command updates the status — run `echo $?` and then `echo "$?"`
separately and the second prints `0`, the first echo's own success.
The circuit of §2.3, catching the unwary.)*

### Lab 2 — filenames don't lie (an argc probe)

`echo` joins its arguments with spaces, hiding token boundaries — but
a command that treats each argument as a *filename* can't lie:

```
mysh> wc 'two words'
wc: two words: No such file or directory        ← ONE token
mysh> wc two words
wc: two: No such file or directory              ← TWO tokens
wc: words: No such file or directory
mysh> wc ""
wc: : No such file or directory                 ← the EMPTY token exists
mysh> FOO='a b'
mysh> wc $FOO
wc: a b: No such file or directory              ← injection, no re-split (§7.3)
```

### Lab 3 — gluing, braces, comments

```
mysh> echo ab'cd'ef
abcdef                       ← quotes toggle modes, never split (§7.2)
mysh> echo ${HOME}
/home/rahul                  ← braced form, delimiter-proof
mysh> echo a#b
a#b                          ← # mid-word is a byte
mysh> echo hi #comment
hi                           ← # at word start is a comment
mysh> echo 'oops
mysh: unmatched single quote ← error door: words freed, line skipped
```

### Lab 4 — photograph the buckets

```sh
gdb shell/mysh
(gdb) break run_pipeline
(gdb) run
mysh> sort < in.txt | uniq -c > out.txt
(gdb) print nstages              # 2
(gdb) print stages[0].argv[0]    # "sort"
(gdb) print stages[0].redir_in   # "in.txt"
(gdb) print stages[1]            # argv {"uniq", "-c"}, redir_out "out.txt",
(gdb) continue                   #   append = 0 — §7.4's diagram, live
```

Every string you print is a borrowed Pip into `tok_t`'s heap words —
`x/s stages[0].argv[0]` and `print tok.w[0]` (one frame up, in
`main`) show the same address.

### Lab 5 — close the crash hole (mandatory)

Reproduce first: `printf 'ls |\n' | ./shell/mysh; echo $?` →
`Segmentation fault`, `139`. Then enforce the grammar's "at least one
word" claim in `parse_pipeline` — two guards. In the `|` branch,
before sealing:

```c
        if (strcmp(w, "|") == 0) {
            if (cur->argc == 0) {
                fprintf(stderr, "mysh: syntax error near '|'\n");
                return -1;
            }
            cur->argv[cur->argc] = NULL;
```

…and after the loop, for the trailing case:

```c
    cur->argv[cur->argc] = NULL;
    if (*nstages > 1 && cur->argc == 0) {
        fprintf(stderr, "mysh: syntax error near '|'\n");
        return -1;
    }
```

Rebuild and re-verify all three invalid sentences: `ls |`, `| ls`,
`a | | b` now print one honest error each, and `echo survived`…
survives. (Bonus: the first guard upgrades `| ls` from silent
discard to a real diagnostic — strictly better.) Add the three lines
to `test_apps.sh` so the hole stays closed: that's the handout's
"invalid syntax cases" deliverable, earned the hard way.

---

## 7.7 Chapter 7 — the rules to keep

> **R40.** Two schools of string slicing: **fence-drop** in place
> (instant, destructive — keep the receipt: `*eq = '\0'` … `*eq = '='`)
> and **measure-and-duplicate** (`malloc(len+1)` + copy + fence =
> `strndup`). Slices that outlive the scan must be duplicates — lexers
> duplicate.
>
> **R41.** Lex with explicit modes (NORMAL / IN_SQ / IN_DQ + `in_word`):
> single quotes are absolute (`'$?'` = two bytes), double quotes are
> literal-plus (`\"` `\\` `\$` escapes, `$` expands), quotes toggle
> modes but never split (`ab'cd'ef` glues), `in_word` makes `""` a
> real empty token, `#` comments only at word boundaries, and overlong
> input is dropped, never overflowed.
>
> **R42.** Expansion is **injection, not re-tokenization**: `$?` /
> `${VAR}` / `$VAR` append into the current word and are never
> rescanned — values with spaces stay one argument (no `$IFS`
> surprises, no quoting superstitions). Unset expands to empty; lone
> `$` is a byte. Pass scanning cursors explicitly (`do_expand(&p, …)`,
> with its step-back contract) — Chapter 5's getopt needed TLS because
> it didn't.
>
> **R43.** Layer once each: characters→tokens (quoting resolved),
> tokens→buckets (structure resolved), buckets→workers (execution).
> Downstream layers *trust* upstream invariants — a parser never
> re-asks what a quote meant.
>
> **R44.** Operators route to **fields, not argv**: `|` seals a bucket
> (NULL fence first!), `<` `>` `>>` consume their neighbor into
> `redir_*`/`append`. Buckets are *views* — borrowed Pips into the
> token shelf, valid exactly until `tok_free` (the §1.10 lease).
> Bounds-check every deal (`TOK_MAX - 1`, `PIPELINE_MAX`) with errors,
> not truncation.
>
> **R45.** A parser earns trust through its **invalid** sentences.
> Every "at least one X" in the grammar is a guard the code must
> enforce: the unguarded empty stage shipped `argv[0] == NULL` to
> `reg_find` and segfaulted the whole in-process shell (verified,
> exit 139). Write the invalid-case tests first; in a one-room shell,
> grammar holes are crash holes.

---

## ⏭ Next: CHAPTER 8 — *Plumbing*

The buckets are parsed, the workers hired — now follow the **bytes**.
Slot 1 gets papered over with `dup2`; the Mirror and the Speedrunners
finally share a scene (and a card-box); and every inter-stage pipe
card turns out to carry a self-destruct stamp — which we will strip,
live, and watch the pipeline deadlock on itself.

---
---

# CHAPTER 8
# Plumbing: File Descriptor Card-Boxes, dup2 Overwrites, and the CLOEXEC Self-Destruct

> *In which slot 1 is papered over and the screen goes quiet, the
> Forking Mirror photocopies a card-box full of the Speedrunners'
> pipe cards, a permission bit betrays which of two plumbings ran —
> and deleting two stamps makes `tr` wait forever for itself.
> (Verified: 5.002 s hang. Restored: 0.003 s.)*

---

## 8.1 The card-box, completed

Chapter 2 introduced cards and bookmarks in ninety seconds; this
chapter runs on the full model, so let's complete it.

Every process owns one **card-box** — the file-descriptor table: an
array of slots indexed by small integers, each slot either empty or
holding a card that points at a kernel **bookmark** (open file
description), which in turn fronts the real object (file, pipe, tty,
socket — Chapters 2–6 collected the whole set). Three completing
facts:

1. **Slots 0/1/2 are convention, not magic.** Nothing in the kernel
   treats them specially; they are a *treaty* — every exec'd program
   since 1971 assumes stdin/stdout/stderr live there. Plumbing is the
   art of honoring that treaty.
2. **`open()` always takes the lowest free slot.** Open a file in a
   fresh shell and it lands on 3 (0/1/2 are taken). This rule is why
   scratch cards in this chapter are always "some slot ≥ 3" — and
   why they must be cleaned up.
3. **Flags live at two different layers**, and `fcntl` has two
   namespaces to match — you have now used both:

| Layer | `fcntl` pair | Flags | Shared? | You used it in |
|---|---|---|---|---|
| the **card** (descriptor) | `F_GETFD` / `F_SETFD` | `FD_CLOEXEC` | per-card — copies don't share it | this chapter (`mysh.c:327`) |
| the **bookmark** (description) | `F_GETFL` / `F_SETFL` | `O_NONBLOCK`, `O_APPEND`, … | shared by *every* card on the bookmark | Chapter 6 (`cmd_fetch.c:106`) |

Hold that table; §8.4 turns the per-card line into the chapter's
finale. And the two cast members own this chapter jointly because
they relate to the card-box in opposite ways: **Speedrunners share
the live box** (every card a thread opens is instantly visible to its
siblings), while **the Mirror photocopies it at the flash** (the
clone gets duplicate cards to the same bookmarks). A mixed pipeline
has both happening at once — §8.3's choreography.

---

## 8.2 The Redirect Overwrite — `apply_redirs`, line by line

Here is the whole function that makes `> out.txt` work for external
commands:

```c
static int apply_redirs(const stage_t *s)
{
    if (s->redir_in) {
        int fd = open(s->redir_in, O_RDONLY);
        if (fd < 0) { fprintf(stderr, "mysh: %s: %s\n", s->redir_in, strerror(errno)); return -1; }
        dup2(fd, STDIN_FILENO); close(fd);
    }
    if (s->redir_out) {
        int flags = O_WRONLY | O_CREAT | (s->append ? O_APPEND : O_TRUNC);
        int fd = open(s->redir_out, flags, 0644);
        if (fd < 0) { fprintf(stderr, "mysh: %s: %s\n", s->redir_out, strerror(errno)); return -1; }
        dup2(fd, STDOUT_FILENO); close(fd);
    }
    return 0;
}
```
*(`shell/mysh.c:159-173`)*

Decode the output branch one token at a time:

**The `open` flags** — `O_WRONLY | O_CREAT | (append ? O_APPEND :
O_TRUNC)`: write-only; create if missing; and here `stage_t.append` —
set by the parser when it saw `>>` (§7.4) — finally cashes out:
`O_TRUNC` empties an existing file at open (`>` semantics — verified:
two `echo … >` writes left only the second), while `O_APPEND` makes
*every* write atomically seek-to-end first (`>>` — verified: the
third line arrived after, not over). Note `O_APPEND` is a *bookmark*
flag from §8.1's table — it travels with the open file description
into the child.

**The `0644`** — permission bits for the *creation* case:
`rw-r--r--`, owner writes, world reads, filtered through the
process's umask. (Pocket the number; a twist below.)

**The `dup2(fd, STDOUT_FILENO)` two-step** — the heart of the
chapter. `dup2(src, dst)` says: *whatever card is in slot `dst`,
destroy it; then place a copy of slot `src`'s card there* — both
cards now tied to the **same bookmark**. Before and after, for
`tr … > up.txt`:

```
        BEFORE                                  AFTER dup2(3, 1); close(3);
   ┌───────────────────┐                       ┌───────────────────┐
   │ 0 ●─▶ tty (in)    │                       │ 0 ●─▶ tty (in)    │
   │ 1 ●─▶ tty (out) ◀─┼── the screen          │ 1 ●─▶ up.txt ◀────┼── papered
   │ 2 ●─▶ tty (err)   │                       │ 2 ●─▶ tty (err)   │   over!
   │ 3 ●─▶ up.txt      │ ← open() took the     │ 3   (empty)       │ ← scratch
   └───────────────────┘   lowest free slot    └───────────────────┘   returned
```

Slot 1 is *physically overwritten*. The program about to be exec'd
doesn't know and doesn't care — it inherits a card-box where the
treaty slot for "output" happens to lead to a file box. Every
`printf` it ever makes now flows into `up.txt`, because `printf` →
stdout → `write(1, …)` → whatever bookmark slot 1 names. That is the
entire mechanism of redirection: **don't change the program; change
what its slot points at.** The trailing `close(fd)` returns the
scratch card — slot 3's job was only to ferry the bookmark to slot 1
(one bookmark, briefly two cards, then one again).

**Where it runs** is as important as what it does: in the *clone*,
inside Chapter 2's customization gap —

```c
            if (g_cmd_fd >= 0) close(g_cmd_fd);
            if (apply_redirs(s) < 0) _exit(1);
            execvp(s->argv[0], s->argv);
```
*(`shell/mysh.c:242-244`)*

— after the flash, before the costume change. The clone rewires *its
own* room and then becomes `tr`. The shell's slot 1 never moved.

### The other plumbing — and the permission bit that betrays it

Internal commands can't use any of this. `dup2` rewires a *room*, and
an in-process `echo` shares the shell's room — papering over slot 1
would redirect the **shell itself**, prompts and all. So `run_inproc`
redirects internals the only safe way, the way the anatomy was built
for (§2.4): hand the command different *parameters*:

```c
        if (s->redir_in) {
            in = owned_in = fopen(s->redir_in, "r");
```
```c
        if (s->redir_out) {
            const char *mode = s->append ? "a" : "w";
            out = owned_out = fopen(s->redir_out, mode);
```
*(`shell/mysh.c:208-217`, trimmed)*

Same `>` syntax, two mechanisms: **dup2 rewires a room; `FILE *`
parameters rewire a call.** Slots are for strangers; parameters are
for family.

And here is the forensic gem, discovered live while verifying this
chapter: *the two plumbings leave different fingerprints.*

```
mysh> echo hi > /tmp/f1                      (internal echo → fopen path)
mysh> tr a-z A-Z < Makefile > /tmp/f2        (external tr → apply_redirs path)
$ ls -l /tmp/f1 /tmp/f2
-rw-rw-r--  /tmp/f1        ← fopen creates with 0666 & ~umask  = 0664
-rw-r--r--  /tmp/f2        ← open(…, 0644) & ~umask            = 0644
```

`fopen` has no mode argument — it asks for `0666` and lets the umask
trim it; `apply_redirs` asks for `0644` explicitly. One `ls -l` tells
you which plumbing ran. (Harmless — but a perfect illustration that
"same behavior" abstractions leak at the edges, and a free debugging
trick: the group-write bit is a confession.)

---

## 8.3 Inter-Thread Pipe Choreography — one card-box, two cast members

Now the full ensemble scene. Take the mixed pipeline

```
mysh> cat Makefile | tr a-z A-Z | wc -l
```

— `cat` and `wc` are registry residents (Speedrunners T1 and T2);
`tr` is a stranger (Mirror clone + costume change). `run_pipeline`
lays two pipes and passes a relay baton:

```c
            int pfd[2];
            if (pipe(pfd) < 0) {
```
```c
            fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
            fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
            cur_out_fd = pfd[1];
            next_in_fd = pfd[0];
```
*(`shell/mysh.c:321-330`, trimmed — stamp both ends at birth, keep
the write end for *this* stage, park the read end as the **baton**
for the next iteration: `cur_in_fd = next_in_fd` at `:308`)*

Each stage's two raw cards then take one of two roads, and the
contrast is the user-visible thesis of this chapter:

| | thread stage (`cat`, `wc`) | fork stage (`tr`) |
|---|---|---|
| how it gets the cards | **shares the live card-box** — `fdopen(fd)` wraps the card in a private `FILE` and the packet carries it to the worker's note-pad (§5.2) | **photocopy at the flash** — inherits every card, then `dup2`s its two onto slots 0/1 |
| slots 0/1/2 touched? | never — streams travel as *parameters* (`a->in`, `a->out`) | yes — that's the *point*; the exec'd binary only knows the treaty slots |
| who closes what | the worker: `fclose(a->out)` = flush + EOF whistle | `dup2`'s implicit close + explicit scratch closes + the stamps (§8.4) |
| why this road | family: a function call can take arguments | stranger: `execvp` can only pass argv, env, and the card-box |

The fork stage's landing sequence, with its load-bearing comment:

```c
                /* dup2 lands the fds on 0/1; the originals close with the
                 * FD_CLOEXEC copies on exec, leaving a clean fd table. */
                if (cur_in_fd  != STDIN_FILENO)
                    { dup2(cur_in_fd,  STDIN_FILENO);  close(cur_in_fd); }
                if (cur_out_fd != STDOUT_FILENO)
                    { dup2(cur_out_fd, STDOUT_FILENO); close(cur_out_fd); }

                execvp(s->argv[0], s->argv);
```
*(`shell/mysh.c:426-433`)*

…and the parent's other half — release immediately, own nothing you
gave away:

```c
            /* Parent: release fds now that the child has them. */
            if (cur_in_fd  != STDIN_FILENO)  close(cur_in_fd);
            if (cur_out_fd != STDOUT_FILENO) close(cur_out_fd);
```
*(`shell/mysh.c:438-440`; plus the break-path cleanup of the dangling
baton: `if (next_in_fd >= 0) close(next_in_fd);` at `:447-448`. R18's
one-owner law, applied to cards: at every instant, each card has
exactly one process-and-purpose responsible for closing it.)*

When the music stops, bytes flow: T1's `fclose` closes pipe A's last
write card → `tr` reads EOF on slot 0 → `tr` exits, and the kernel
closes its slot 1 (pipe B's last write card) → T2's `getline` returns
−1 → `wc` prints its count. Chapter 5's EOF cascade, now crossing
**room boundaries** — threads and processes in one relay, joined by
nothing but card discipline.

---

## 8.4 The CLOEXEC Self-Destruct — and the deadlock we resurrected

Why were both pipe ends stamped at birth? Because of what the Mirror
does to a *shared* card-box. The comment above the stamps says it
plainly:

```c
            /*
             * Create the inter-stage pipe.  O_CLOEXEC ensures that any
             * fork'd external-command child exec's with these fds closed,
             * preventing it from holding a write end open and blocking EOF.
             */
```
*(`shell/mysh.c:316-320`)*

**`FD_CLOEXEC` semantics, precisely:** the stamp is a *card* flag
(§8.1's table). It **survives the flash** — the photocopy carries the
stamp — and it **fires at the costume change**: at `execvp`, every
stamped card in the new image is destroyed before the new program's
first instruction. And the linchpin that makes the whole choreography
sound: **`dup2` copies do *not* inherit the stamp.** That asymmetry is
exactly what `mysh.c:426`'s comment is exploiting — the `dup2`'d cards
on slots 0/1 are *unstamped* (they must survive into `tr`), while the
stamped originals on slots ≥ 3 all burn at exec, "leaving a clean fd
table."

Now the disaster the stamps prevent. At the instant `tr`'s Mirror
flashes, the shared card-box also contains **pipe A's write end —
inside Speedrunner T1's `FILE`**. The clone photocopies it. Without
the stamp, that copy survives the costume change, and:

```
   without the stamp on A.w:

     T1 cat ──A.w (FILE)──▶ ║ pipe A ║ ──A.r→slot 0──▶ tr (exec'd)
                            ║        ║ ◀──A.w copy, slot ≥3, forgotten── tr ALSO
   cat finishes → T1 fcloses A.w → kernel counts write cards: "tr
   still holds one" → no EOF on pipe A → tr keeps waiting for input
   → tr is waiting for ITSELF.   ⏳ forever
```

This is not a theory. We deleted the two stamp lines, rebuilt, and
ran the pipeline — then restored them:

```sh
$ sed -i '/fcntl(pfd\[/d' shell/mysh.c && make
$ time (printf 'cat Makefile | tr a-z A-Z\nexit\n' | timeout 5 ./shell/mysh >/dev/null)
real    0m5.002s        # hung until timeout killed it — exit 124
$ git checkout -- shell/mysh.c && make
$ time (printf 'cat Makefile | tr a-z A-Z\nexit\n' | timeout 5 ./shell/mysh >/dev/null)
real    0m0.003s        # exit 0
```

Two `fcntl` lines are the difference between three milliseconds and
forever. Note the deadlock's cruelest property: **all the output
appears** — pipes stream, so the uppercased text flows through before
the hang; the pipeline produces everything and then refuses to
finish. That symptom — *complete output, no completion* — is the
signature of a leaked write card, and `ls -l /proc/<pid>/fd` finds
the culprit faster than any debugger.

### The showdown: the stamp vs. the manual close

Chapter 2 promised this comparison. Your shell uses *both* hygiene
idioms, and each is used where only it works:

| | `FD_CLOEXEC` stamp | manual `close()` in the clone |
|---|---|---|
| style | declarative — set once at birth | imperative — repeat at every fork site |
| fires | at the costume change (`exec`) | when the code runs |
| fork **without** exec | **card survives** — stamp never fires | works — the only option |
| who uses it | every pipe end and redirect fd in `run_pipeline` (`:327-328`, `:297`, `:303`, `:341`, `:350`) | `g_cmd_fd`, at all three Mirror Machines (`:242`, `:424`, `:759`) |
| why there | stages are created in a loop; children exec immediately; the cards must live *until* exec (the child needs them for `dup2`!) and die *at* it — exactly the stamp's timing | **Mirror Machine 3 never execs** — the background clone runs shell code dressed as `mysh` (§2.1), so a stamp on `g_cmd_fd` would never fire; only an explicit close protects the command stream |

One refinement worth knowing as you grow this shell: the stamp is
applied a few instructions *after* `pipe()` creates the cards. In
today's mysh that gap is harmless — pipes are made and forks are
flashed by the same (main) thread, strictly in sequence. But in a
program where *another* thread can fork during the gap, the photocopy
catches unstamped cards — which is why modern Linux offers atomic
spellings: `pipe2(pfd, O_CLOEXEC)` and `open(…, O_CLOEXEC)`, stamping
at birth with no gap at all. (The comment at `:317` already says
"O_CLOEXEC" — the code is one syscall rename away from its own
ideal.)

---

## 8.5 Lab — paper over slots, photograph boxes, resurrect the hang

### Lab 1 — the permission fingerprint

```
mysh> echo hi > /tmp/f1
mysh> tr a-z A-Z < Makefile > /tmp/f2
mysh> exit
$ ls -l /tmp/f1 /tmp/f2
```

`-rw-rw-r--` vs `-rw-r--r--` (verified): internal `echo` rode the
`fopen("w")` plumbing (0666 & ~umask), external `tr` rode
`apply_redirs`' `open(…, 0644)`. One command, `ls -l`, tells you
which of §8.2's two schools handled each file.

### Lab 2 — photograph a papered-over card-box

```
mysh> tr a-z A-Z < /dev/zero > /dev/null &
[bg] 51234
```
From another terminal:
```sh
ls -l /proc/$(pgrep -x tr)/fd
# 0 -> /dev/zero        ← slot 0, papered by dup2 (via apply_redirs)
# 1 -> /dev/null        ← slot 1, papered
# 2 -> /dev/pts/…       ← stderr untouched: errors still reach you
kill $(pgrep -x tr)
```

The §8.2 diagram, photographed in a live process — and notice what's
*absent*: no slot 3. The scratch card was closed; the box is clean.

### Lab 3 — strace the two plumbings

```sh
printf 'tr a-z A-Z < Makefile > /tmp/up.txt\nexit\n' | \
  strace -f -e trace=openat,dup2,close,execve ./shell/mysh 2>&1 | grep -E 'openat.*(Makefile|up)|dup2|execve.*tr'
```

Expect, in the child: `openat(… "Makefile", O_RDONLY) = 3` →
`dup2(3, 0)` → `close(3)` → `openat(… "up.txt",
O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3` → `dup2(3, 1)` → `close(3)` →
`execve("/usr/bin/tr", …)`. Then rerun with `echo hi > /tmp/f1`
instead: **no `dup2` appears at all** — the internal path opens the
file and never touches a slot. Two schools, two transcripts.

### Lab 4 — resurrect the deadlock (then put the stamps back)

```sh
sed -i '/fcntl(pfd\[/d' shell/mysh.c && make
time (printf 'cat Makefile | tr a-z A-Z\nexit\n' | timeout 5 ./shell/mysh > /dev/null)
# real ≈ 5.002s, exit 124 — all output produced, completion never comes
git checkout -- shell/mysh.c && make     # restore: real ≈ 0.003s
```

While it hangs (drop the `timeout` and run it interactively to
linger), photograph the culprit from another terminal:
`ls -l /proc/$(pgrep -x tr)/fd` — there it is, a `pipe:[…]` card on
some slot ≥ 3 with the *same inode* as slot 0: `tr` holding the write
end of the pipe it's reading. The deadlock, in one `ls`.

### Lab 5 — `O_TRUNC` vs `O_APPEND`

```
mysh> echo a > /tmp/mc8
mysh> echo b > /tmp/mc8        # O_TRUNC (well, fopen "w"): start over
mysh> echo c >> /tmp/mc8       # append mode: seek-to-end every write
mysh> cat /tmp/mc8
b
c
```

(Verified.) `stage_t.append` — born as two characters `>>` in §7.2's
lexer, routed to a field in §7.4's parser — ends its journey here as
a single flag bit choosing between two open modes.

---

## 8.6 Chapter 8 — the rules to keep

> **R46.** `fcntl` has two namespaces because flags live at two
> layers: `F_SETFD` flips **card** flags (`FD_CLOEXEC` — private to
> one descriptor), `F_SETFL` flips **bookmark** flags (`O_NONBLOCK`,
> `O_APPEND` — shared by every card on the description). Know which
> layer you're flipping before you flip it.
>
> **R47.** `dup2(src, dst)` = destroy whatever card is in slot `dst`,
> install a copy of `src`'s card (same bookmark) — and the copy is
> born **unstamped**. The idiom is a two-step: `open` → `dup2` →
> `close` the scratch card. One bookmark, briefly two cards, then one.
>
> **R48.** Slots 0/1/2 are a treaty with strangers. Rewire **rooms**
> with `dup2` (fork branch, in the customization gap); rewire
> **calls** with `FILE *` parameters (in-process branch — you cannot
> paper over the shell's own slot 1). Same `>` syntax, two plumbings —
> and the permission bits (0664 vs 0644) confess which one ran.
>
> **R49.** A pipe delivers EOF only when its **last write card**
> dies. Account for every card at every moment: parent releases its
> copies the instant the child owns them; loop-break paths close the
> dangling baton; one owner per card, always.
>
> **R50.** `FD_CLOEXEC` survives the flash and fires at the costume
> change — exactly right for cards that children need *until* exec
> and must not keep *after*. It is useless for fork-without-exec
> (Mirror Machine 3), which is why `g_cmd_fd` takes manual closes
> instead. Use the stamp for plumbing, the close for clones that stay
> dressed as you — your shell uses both, each where only it works.
> (Atomic spellings — `pipe2`, `O_CLOEXEC` — close the
> stamp-after-birth gap when other threads can fork.)
>
> **R51.** The leaked-write-card deadlock has a signature: **all the
> output, no completion** (verified: 5.002 s vs 0.003 s, two `fcntl`
> lines apart). Diagnose with `ls -l /proc/<pid>/fd` — a reader
> holding a `pipe:[N]` write card matching its own slot 0 is waiting
> for itself.

---

## ⏭ Next: CHAPTER 9 — *Terminal Wrestling*

Every chapter so far trusted the terminal to deliver tidy lines. It
never did — a hidden kernel doorman was editing your keystrokes,
buffering your bytes, and turning Ctrl-C into an alarm before mysh
ever saw it. Time to meet him, photocopy his rulebook, cross out
eight of his rules — and hang a safety net for the day the shell
dies mid-edit.

---
---

# CHAPTER 9
# Terminal Wrestling: Taming the Keyboard with Termios Raw Mode

> *In which we meet the Cooked Gatekeeper who has been editing your
> typing since 1971, photocopy his rulebook and cross out eight rules,
> flip the switch for milliseconds at a time — and inspect the safety
> net for the day the shell dies mid-edit, including the one hole the
> net honestly has.*

---

## 9.1 The Cooked Gatekeeper — the editor you never hired

A new character, because there has been an invisible one in every
chapter so far. Between your keyboard and every `read(0, …)` your
shell has ever made sits a kernel layer called the **tty line
discipline** — in this book, **the Cooked Gatekeeper**:

```
   keyboard ──wire──▶ [tty driver] ──▶ THE GATEKEEPER'S DESK ──▶ read(fd 0)
                                        (kernel line discipline)
   In "cooked" (canonical) mode, at his desk, the Gatekeeper:
     · ECHOES every key back to the screen himself (you never drew it)
     · BUFFERS the line — read() gets NOTHING until Enter
     · EDITS at the desk: Backspace erases, Ctrl-U kills the line
     · converts Ctrl-C into a fire alarm (SIGINT) — not a byte
     · translates Enter's '\r' into '\n'   (ICRNL)
     · freezes/thaws output on Ctrl-S / Ctrl-Q   (IXON)
```

The Gatekeeper is why a 1970s teletype, a serial port, and your
terminal emulator all "just work": programs receive tidy,
newline-terminated, pre-edited lines, one `read()` per line, and the
kernel pays for the editing. Every cooked-mode `fgets` in this book —
script mode, pipe mode, Chapter 2's command loop — was reading the
Gatekeeper's finished output.

But his desk has a hard limit, and your linedit header states it
exactly:

> *"Canonical ('cooked') terminal input only allows editing at the end
> of the line: the kernel buffers the line and backspace is the only
> editing key. Arrow keys arrive as multi-byte escape sequences (e.g.
> ESC [ D for Left) which the line discipline happily stores into the
> buffer as garbage."*
> *(`shell/linedit.c:4-7`)*

The Gatekeeper edits **only at the line's tail**. Press Left-arrow and
your terminal sends three bytes — `ESC [ D` — which he doesn't
understand and dutifully *stores as text*. A shell that wants
cursor-movement editing (Chapter 10's whole point) must therefore send
the Gatekeeper on break and do *all of his jobs itself*. That
hand-over is raw mode, and it is a formal, reversible, carefully
scoped transaction — this chapter.

---

## 9.2 The rulebook swap — `raw_enable()`, flag by flag

The Gatekeeper's standing orders live in a `struct termios` — the
**rulebook**: four flag words (`c_iflag` input rules, `c_oflag` output
rules, `c_lflag` "local" desk-behavior rules, `c_cflag` hardware) plus
the control-character array `c_cc`. Here is the entire swap:

```c
static int raw_enable(int fd)
{
    if (tcgetattr(fd, &g_orig_termios) == -1) return -1;

    /* Restore the terminal even if the shell exits from inside the editor. */
    static int registered = 0;
    if (!registered) { atexit(raw_disable); registered = 1; }

    struct termios raw = g_orig_termios;
    raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;   /* read() blocks until at least one byte */
    raw.c_cc[VTIME] = 0;
    /* c_oflag stays untouched: "\n" -> CRLF post-processing keeps working. */

    if (tcsetattr(fd, TCSADRAIN, &raw) == -1) return -1;
    g_raw    = 1;
    g_raw_fd = fd;
    return 0;
}
```
*(`shell/linedit.c:81-100`)*

The shape first, then the rules. **`tcgetattr` photocopies the current
rulebook** into `g_orig_termios` (a global — Chapter 4 explained why:
`atexit` has no context slot, so the restore state must be findable
globally, `linedit.c:67-69`). Then the cardinal idiom: **amend a
copy** (`struct termios raw = g_orig_termios;`), never the original
and never a from-scratch struct — the photocopy is both your starting
point *and* your way home. (Chapter 6 did the identical dance with
`F_GETFL`/`F_SETFL`: R37, *restore what you toggle*, needs a saved
original to restore.)

Now the eight crossed-out rules — and read this table as a **transfer
of duties**, because every job the Gatekeeper stops doing becomes
*your* job:

| Cleared | The Gatekeeper stops… | …so linedit must |
|---|---|---|
| `ECHO` | echoing keys to the screen | draw every character itself (`le_refresh`, Ch 10) |
| `ICANON` | buffering until Enter & desk-editing | receive raw bytes instantly; implement Backspace/Ctrl-U/Ctrl-W by hand |
| `ISIG` | converting Ctrl-C/Ctrl-\\/Ctrl-Z into signals | handle **byte 3** itself (`le_edit case 3:` — abandon line); Ctrl-Z arrives as byte 26, ignored |
| `IEXTEN` | extended tricks (Ctrl-V literal-next…) | nothing — declined politely |
| `ICRNL` | translating Enter's `'\r'` to `'\n'` | accept `'\r'` as Enter — exactly why `le_edit`'s accept case reads `case '\r': /* Enter (ICRNL is off) */` (`linedit.c:374`) |
| `IXON` | freezing output on Ctrl-S (the famous "my terminal hung!") | treat Ctrl-S/Q as ignorable bytes — your shell *cannot* be accidentally frozen |
| `BRKINT`, `INPCK`, `ISTRIP` | serial-era break/parity/7-bit handling | nothing — `ISTRIP` especially *must* go or every UTF-8 byte loses its high bit |

Two settings are tuned rather than cleared. **`VMIN = 1, VTIME = 0`**
defines what a raw `read()` *means*: block until **at least one byte**
arrives, no timer — the editor's heartbeat is "give me the next
keystroke, however long it takes." (When linedit *does* need a timed
read — is this lone ESC a keypress or the start of an arrow sequence?
— it doesn't fiddle `VTIME`; it pairs the blocking read with
Chapter 6's `poll(…, 50)`: R38, one tool at every timescale.)

And one deliberate *non*-change: **`c_oflag` stays intact**
(`linedit.c:94`). Most raw-mode tutorials clear `OPOST` and doom
themselves to writing `"\r\n"` by hand forever; linedit keeps the
kernel's `'\n'` → CRLF output post-processing, so ordinary `printf`s
keep landing correctly even mid-raw-mode. Take only the powers you
need — the lighter the wrestling hold, the fewer ways it can go wrong.

---

## 9.3 The Safe Switch Protocol — raw for milliseconds at a time

Here is the entire lifetime of raw mode in your shell, end to end:

```c
    if (isatty(ifd) && isatty(STDOUT_FILENO) && !term_is_dumb()) {
        char *draw = malloc(strlen(prompt) + bufsz + 48);
        if (draw && raw_enable(ifd) == 0) {
            /* Anything buffered (e.g. a previous command's output) must hit
             * the screen before we start drawing with raw write()s. */
            fflush(stdout);
```
```c
            int rc = le_edit(&l);

            raw_disable();
            free(draw);
```
*(`shell/linedit.c:454-470`, trimmed)*

**The switch flips around exactly one line-read.** Raw at the prompt;
cooked again *before* `linedit_read` even returns — therefore cooked
during tokenizing, parsing, and every command the line triggers. The
header comment states the contract: *"…before `linedit_read` returns,
so commands always run in the original mode"* (`linedit.c:38-39`).
Three reasons this narrow window is load-bearing, in rising order of
subtlety:

1. **Commands expect the Gatekeeper.** An interactive `cat`, a
   password prompt, anything reading stdin — written for cooked mode,
   echo and all.
2. **Chapter 2's entire Ctrl-C story presupposes it.** Remember the
   dance: shell ignores SIGINT, children restore `SIG_DFL`, Ctrl-C
   kills the child and the prompt survives. But *who generates*
   SIGINT? **The Gatekeeper — and only when `ISIG` is on.** If the
   terminal stayed raw while `sleep 100` ran, Ctrl-C would arrive
   nowhere as a signal — just an unread byte 3 — and the child would
   be uninterruptible. The §2.2 signal choreography works *because*
   the safe-switch protocol has put the Gatekeeper back at his desk
   before any command runs.
3. **Crash exposure.** Your shell is one room (R13). Every millisecond
   spent in raw mode is a millisecond in which an unexpected death
   leaves the terminal wrecked (§9.4). Minimal window, minimal
   exposure.

Notice also the *ordering discipline* around the flip. The fallible
`malloc` happens **before** `raw_enable` — if it fails, the shell
falls back to cooked input without ever touching the rulebook. The
`fflush(stdout)` happens **immediately after** — Chapter 2 taught that
stdio furniture holds bytes; once the editor starts drawing with raw
`write()`s, any *buffered* prompt text would interleave out of order.
Empty the furniture, then draw. (And the cooked fallback at
`linedit.c:481-487` — `fputs` + `fgets`, "identical to the shell's
historical prompt behaviour" — is what scripts, pipes, and `TERM=dumb`
terminals get: `term_is_dumb()` checks for terminals that can't
interpret cursor escapes, `linedit.c:436-443`. Degrade by *choosing
the older contract*, never by half-working.)

### Why `TCSADRAIN` — the three ways to apply a rulebook

`tcsetattr`'s second argument decides *when* the new rules take
effect, and the choice encodes respect for in-flight data:

| Mode | Output queue (pending draws) | Input queue (type-ahead) | Verdict for linedit |
|---|---|---|---|
| `TCSANOW` | rules change **mid-stream** | kept | a redraw still in the kernel's outbox could be re-interpreted under new rules — garbling risk |
| **`TCSADRAIN`** | **drained first** — every queued byte leaves under the *old* rules | **kept** | ✓ both directions safe |
| `TCSAFLUSH` | drained | **discarded** | destroys exactly the wrong thing — see below |

The input column is the one your code's comments care about most:

```c
static void raw_disable(void)
{
    if (g_raw) {
        /* TCSADRAIN: let queued output finish drawing, but keep any
         * type-ahead the user has already entered. */
        tcsetattr(g_raw_fd, TCSADRAIN, &g_orig_termios);
        g_raw = 0;
    }
}
```
*(`shell/linedit.c:71-79`)*

Concrete scenario, from the header (`linedit.c:36-38`): you **paste
two command lines** at the prompt. The editor accepts line 1 on its
`'\r'`; line 2 is still sitting in the kernel's input queue,
unread. The restore-to-cooked happens right then — with `TCSAFLUSH`,
line 2 would be *vaporized*; with `TCSADRAIN`, it survives the mode
switch, the Gatekeeper (back at his desk) cooks it normally, and the
next prompt reads it. Pastes work in mysh because of one constant's
worth of care.

---

## 9.4 The Emergency Exit — `atexit`, the net, and the hole in the net

What does a *wrecked* terminal look like? Suppose a process flips to
raw and dies without restoring: the Gatekeeper is still on break.
Your next shell prompt arrives, you type — **nothing appears** (no
`ECHO`). Enter half-works, Ctrl-C does nothing (`ISIG` off — no alarm
is ever raised). The terminal isn't crashed; it's obeying rules
nobody remembered setting. The rescue ritual, which every Unix hand
eventually learns, is to type — *blind* —

```
reset⏎
```

(the `reset(1)` program reinitializes the tty rulebook). Your shell
hangs a net so its users never need the ritual:

```c
    /* Restore the terminal even if the shell exits from inside the editor. */
    static int registered = 0;
    if (!registered) { atexit(raw_disable); registered = 1; }
```
*(`shell/linedit.c:86-88`)*

Three small pieces of craftsmanship make this net trustworthy:

- **Registered once, structurally** (`static int registered`): the
  shell calls `raw_enable` at every prompt — thousands of times per
  session — but `atexit`'s handler table is finite (C guarantees only
  32 slots). One flag, one registration, forever.
- **Idempotent by guard** (`if (g_raw)` in `raw_disable`): on the
  normal path the handler runs *twice* — once explicitly at
  `linedit.c:469`, once again when the process eventually exits — and
  the second call must be a harmless no-op. The flag makes restore
  safe to call from anywhere, any number of times. (R20's
  free-then-NULL, reborn as restore-then-clear.)
- **Clone-proof by Chapter 2's rules**: a Mirror clone that fails
  `execvp` calls `_exit(127)` — which *skips* atexit handlers (R10) —
  so a dying clone never fights the parent over the shared physical
  terminal. And Machine 3's background clone, which *does* call
  `exit(rc)`, photocopied `g_raw` at flash time — always `0`, since
  commands only run after the prompt restored — so its handler run
  no-ops. The little flag quietly coordinates three different exit
  protocols.

The net's coverage: **any `exit()` from anywhere, forever.** Today no
code path calls `exit()` mid-edit — the net catches nothing. But the
shell is a living codebase: the day some future command, library, or
assert calls `exit()` while `g_raw == 1`, the terminal still comes
back cooked. That is what "structural" means — the guarantee rides
the *exit machinery itself*, not the discipline of every future
contributor.

### The honest hole

`atexit` runs on `exit()` and `return` from `main` — **and on nothing
else.** Death by signal — `SIGTERM` from a `kill`, `SIGSEGV` from a
bug, `SIGKILL` from an impatient admin — bypasses the handler table
entirely. Kill mysh mid-edit from another terminal and you get the
broken-terminal experience, net or no net (Lab 4 makes you do it,
once, so you've seen it). Note how narrowly Chapter 7's discovered
crash missed this: the `ls |` segfault fires in `run_pipeline` —
*after* `linedit_read` restored the terminal — so even that bug left
the tty intact. Timing luck, not design.

The professional patch (and your Lab 4 exercise): install handlers
for the catchable terminators —

```c
static void on_fatal_signal(int sig)
{
    raw_disable();                      /* idempotent — safe from anywhere */
    signal(sig, SIG_DFL);
    raise(sig);                         /* die honestly, with the right status */
}
```

— registered for `SIGTERM`/`SIGHUP` when raw mode first engages.
Restore, then re-raise so the exit status still says "killed by
signal" (Chapter 2's `WIFEXITED` consumers deserve the truth).
`SIGKILL` remains uncatchable by design; for that, there will always
be `reset`.

---

## 9.5 Lab — photograph the Gatekeeper, then break a terminal on purpose

### Lab 1 — the rulebook swap, photographed live

`stty -a` prints the current rulebook. Take three photographs of the
*same* terminal from a second terminal (find the first one's device
with `tty`, e.g. `/dev/pts/3`):

```sh
# 1. victim terminal sitting at a bash prompt:
stty -a -F /dev/pts/3 | head -4      # icanon echo isig icrnl … (cooked)

# 2. victim now sitting at a mysh> prompt (mid-edit, raw):
stty -a -F /dev/pts/3 | head -4      # -icanon -echo -isig -icrnl min=1 time=0

# 3. victim running:  mysh> sleep 5
stty -a -F /dev/pts/3 | head -4      # icanon echo isig … — cooked again!
```

Photograph 3 is §9.3's safe-switch protocol caught in the act: raw at
the prompt, cooked the instant a command runs.

### Lab 2 — Ctrl-C as byte 3 vs. Ctrl-C as alarm

At a `mysh>` prompt, type some text, then Ctrl-C: the editor prints
`^C`, abandons the line, and gives a fresh prompt — **the shell did
not die**, because no signal existed; `le_edit` handled byte 3 as
data (`linedit.c:378-384`). Now run `sleep 100` and press Ctrl-C: the
*sleep* dies instantly and the prompt returns — because the terminal
is cooked again, the Gatekeeper raised a real SIGINT, the shell
ignored it (`mysh.c:553`), and the child's restored `SIG_DFL` accepted
it. One key, two completely different mechanisms, chosen by which
mode the terminal is in at that instant.

### Lab 3 — break a terminal on purpose (sacrificial tab)

In a throwaway terminal:

```sh
stty -echo -icanon       # send the Gatekeeper on break, walk away
date                     # type it blind — no echo; output still appears
reset                    # type THIS blind too, press Enter
```

Thirty seconds of the broken-terminal experience teaches more respect
for `raw_disable` than any paragraph. (`reset` re-initializes the
rulebook; `stty sane` is the gentler sibling.)

### Lab 4 — find the hole in the net, then patch it

At a `mysh>` prompt (mid-edit, raw), from another terminal:

```sh
kill $(pgrep -x mysh)        # SIGTERM — death WITHOUT atexit
```

The victim terminal is now wrecked (Lab 3 prepared you; run `reset`).
You have empirically located the boundary of `atexit`'s guarantee.
**Exercise:** add §9.4's `on_fatal_signal` handler (register
`SIGTERM` and `SIGHUP` inside `raw_enable`, next to the `atexit`
registration), rebuild, repeat the kill — the prompt dies but the
terminal survives. Restore-then-re-raise; check `echo $?` in the
parent reports `143` (128 + SIGTERM), unchanged.

---

## 9.6 Chapter 9 — the rules to keep

> **R52.** Between the keyboard and `read(0, …)` sits the line
> discipline — the Cooked Gatekeeper: echo, line buffering,
> tail-of-line editing, Ctrl-C→SIGINT conversion, CR→NL, flow
> control. Cooked mode = he works, you get lines; raw mode = you get
> bytes **and inherit every job he was doing**.
>
> **R53.** Treat `termios` as a rulebook to photocopy, amend, and
> restore: `tcgetattr` → copy → clear `ECHO|ICANON|ISIG|IEXTEN` (+
> legacy iflags; `ISTRIP` kills UTF-8) → `tcsetattr` → restore the
> photocopy. Never build one from scratch; leave `c_oflag` alone so
> `'\n'` still lands as CRLF. `VMIN=1, VTIME=0` = block for exactly
> one byte; for timed reads, `poll` (R38), not `VTIME` fiddling.
>
> **R54.** Clearing a rule transfers the duty, never deletes it:
> `-ISIG` makes Ctrl-C *your* byte 3 to handle; `-ECHO` makes every
> visible character *your* redraw. Budget for the duties before
> taking the powers.
>
> **R55.** Minimize the raw window: flip around exactly one line-read,
> restore before any command runs — children's Ctrl-C *generation*
> (not just handling) depends on the Gatekeeper being back on duty.
> Switch with `TCSADRAIN`: queued output drains under old rules,
> pasted type-ahead survives (`TCSAFLUSH` eats it, `TCSANOW` risks
> garbling). `fflush` stdio before mixing in raw `write()`s.
>
> **R56.** Make the restore structural: `atexit(raw_disable)`,
> registered once (`static` flag — atexit slots are finite),
> idempotent (`if (g_raw)` — runs twice on the normal path), and
> clone-coordinated for free (`_exit` skips it; photocopied
> `g_raw == 0` no-ops it). A guarantee that rides the exit machinery
> outlives the discipline of future contributors.
>
> **R57.** Know the net's hole: `atexit` ≠ signals. SIGTERM/SIGSEGV
> mid-edit still wrecks the tty — patch with a restore-and-re-raise
> handler for the catchable terminators, and teach your fingers the
> blind `reset⏎` ritual for everything else.

---

## ⏭ Next: CHAPTER 10 — *The Line Editor*

The Gatekeeper is on break and every duty is ours. Pip returns to
keep the books — cursor as a house number, not a string — while a
six-byte burst becomes Ctrl-Left, a fifty-millisecond clock tells ESC
from an arrow, and one terminal row redraws itself perfectly, forever.

---
---

# CHAPTER 10
# The Line Editor: Custom Key Maps, Escape Sequence Decoding, and Horizontal Redraw Loops

> *In which Pip trades strings for house numbers, a digit-collector
> pointer switches targets mid-stream, fifty milliseconds separate the
> Escape key from a six-byte burst — and one row of the terminal is
> redrawn so carefully it never once wraps.*

---

## 10.1 The editor's state — `le_t` and the index discipline

Chapter 1 introduced the pointer/capacity/length trio using this very
struct; the editor completes it into a **quartet** by adding the
cursor:

```c
typedef struct {
    int         ifd, ofd;   /* tty file descriptors                          */
    char       *buf;        /* caller's line buffer                          */
    size_t      bufsz;      /* its capacity (including the NUL)              */
    size_t      len;        /* current line length                           */
    size_t      pos;        /* cursor index into buf, 0..len                 */
    const char *prompt;
    size_t      plen;       /* strlen(prompt)                                */
    char       *draw;       /* scratch buffer for one refresh frame          */
} le_t;
```
*(`shell/linedit.c:151-160`)*

Notice what `pos` and `len` are **not**: pointers. Pip could have
tracked the cursor as a `char *` into `buf` — Chapter 5's getopt
tracked its scan position exactly that way (`place`). Instead the
editor uses **indexes** — house numbers, not Pips — and the choice is
load-bearing, because these numbers do double duty in *two
coordinate systems*:

- **buffer arithmetic**: `buf + pos` for every `memmove` (§1.4's
  exhibits — `le_insert`'s gap-opening, `le_backspace`'s gap-closing —
  were taken from this file);
- **screen arithmetic**: `plen + pos` *is the cursor's terminal
  column* (§10.3). A pointer can't be added to a prompt length; an
  index can.

One number, two geometries — that's why house numbers win here.
(Rule of thumb: indexes when you do arithmetic across domains or the
buffer might move; pointers when you only ever walk. Both idioms are
in your codebase; now you know how to choose.)

The invariants every primitive maintains — verify them against any
editing function in the file:

```
    0 ≤ pos ≤ len < bufsz          and          buf[len] == '\0'  always
```

The kills show the discipline tersely:

```c
static void le_kill_to_start(le_t *l)            /* Ctrl-U */
{
    memmove(l->buf, l->buf + l->pos, l->len - l->pos);
    l->len -= l->pos;
    l->pos  = 0;
    l->buf[l->len] = '\0';
    le_refresh(l);
}
```
*(`shell/linedit.c:241-248`; Ctrl-K just truncates `len = pos`; Ctrl-W
computes `word_left` then does the same slide)*

— move the bytes, fix `len` and `pos`, replant the fence, redraw. And
word motion shares its *definition of a word* with Chapter 7:

```c
/* Word boundaries: blanks separate words (matches the tokenizer's view). */
static size_t word_left(const le_t *l, size_t pos)
```
*(`shell/linedit.c:250-251`)*

Ctrl-Left lands exactly where `tok_split` would cut — the editor and
the lexer agree about what a "word" is because the comment *made* them
agree. UX consistency is a cross-reference, written down.

---

## 10.2 The escape-sequence decoder — a grammar with a stopwatch

Here is the problem in raw bytes. With the Gatekeeper on break,
"one keypress" is a fiction:

```
   key: Left            key: Ctrl-Left                key: Escape alone
   wire: 1B 5B 44       wire: 1B 5B 31 3B 35 44       wire: 1B …silence…
         ESC [  D             ESC [  1  ;  5  D             ESC
         └ ~1 ms burst ┘      └───── one burst ─────┘       └─ 50 ms pass →
                                                              it was just ESC
```

Three different keys, *identical first byte*. No amount of grammar can
distinguish them — the disambiguator is **time**, and your code
documents the physics:

```c
/* Milliseconds to wait for the continuation bytes of an escape sequence.
 * A terminal sends a whole arrow-key sequence in one burst, so a short
 * window cleanly separates "pressed the Escape key" from "pressed Left". */
#define ESC_BYTE_TIMEOUT_MS 50
```
*(`shell/linedit.c:60-63`)*

The stopwatch is `read_byte_timeout` (`linedit.c:115-127`) — a
`poll(&pfd, 1, ms)` wrapped around a one-byte read. Mark the tally:
this is **`poll`'s third timescale in the book** (R38) — 5000 ms for
continents (fetch), 50 ms for keyboard electronics, and the contrast
is the lesson: same syscall, tuned to the physics of what's being
awaited.

### The CSI grammar — Chapter 7 in miniature

After `ESC [`, the terminal speaks a tiny formal language, and the
comment in your decoder cites the standard:

```c
        /* CSI syntax (ECMA-48): any parameter bytes 0x30-0x3F and
         * intermediate bytes 0x20-0x2F, then one final byte 0x40-0x7E.
         * Track up to two numeric parameters ("1;5C" -> p1=1, p2=5);
         * everything else is consumed untracked. */
        int p1 = 0, p2 = 0, *cur = &p1;
        while (b >= 0x20 && b <= 0x3F) {
            if (b >= '0' && b <= '9') *cur = *cur * 10 + (b - '0');
            else if (b == ';')        { cur = &p2; *cur = 0; }
            if (!read_byte_timeout(l->ifd, &b, ESC_BYTE_TIMEOUT_MS)) return -1;
        }
        if (b < 0x40 || b > 0x7E) return b;            /* malformed sequence */
```
*(`shell/linedit.c:309-319`)*

In BNF (Chapter 7's notation, paying rent):

```
   csi-seq    ::= ESC '[' param-byte* final-byte
   param-byte ::= 0x30–0x3F            digits, ';', …
   final-byte ::= 0x40–0x7E            'A'…'D', 'H', 'F', '~', …
```

And savor the **switching Pip** — this chapter's star pointer move.
`cur` is aimed at `p1`; digits flow through `*cur = *cur * 10 +
(b - '0')` — the classic atoi-in-flight — into whichever box `cur`
faces; the `';'` *re-aims the Pip at `p2`* and the very same line of
code starts filling the second parameter. Parsing `"1;5"` runs one
accumulator statement over two destinations, selected by a pointer.
That is §1.2's "re-tie the string" doing real work in six lines.

Then the dispatch — where `p2 == 5` (the `;5` modifier = Ctrl held)
turns an arrow into a word-jump:

```c
        switch (b) {
        case 'C':                                      /* Right */
            if (p2 == 5)              le_move(l, word_right(l, l->pos));
            else if (l->pos < l->len) le_move(l, l->pos + 1);
            break;
```
*(`shell/linedit.c:321-325`; `'D'` mirrors it; `'H'`/`'F'` are
Home/End; `'~'` dispatches on `p1` for the vt-style variants — `1~`/
`7~` Home, `4~`/`8~` End, `3~` Delete, `:332-336`; a parallel SS3
branch at `:340-349` handles `ESC O` application-mode arrows)*

Everything well-formed but unrecognized — Up/Down (no history yet),
PgUp/PgDn, F-keys — is **consumed silently**: the sequence is parsed
to completion and dropped, so pressing F5 never types `15~` into your
command. Unknown ≠ unparsed.

### Malformed sequences — reject without swallowing

The decoder's return contract (documented at `linedit.c:293-298`) is
the part most hand-rolled editors get wrong: `-1` means *fully
consumed*; **a non-negative value is a byte that turned out not to
belong** — handed back so it isn't lost. Two ways that happens:
silence mid-sequence (the stopwatch expires → `-1`, the lone ESC is
simply dropped), or an *impossible byte* where a sequence byte
belongs — caught by `if (b < 0x40 || b > 0x7E) return b;` and by the
ESC-then-control check at `:304`. The caller completes the rescue:

```c
        case 27: {                       /* ESC: arrow keys & friends */
            int leftover = le_escape(l);
            if (leftover >= 0) {         /* malformed sequence ended in a
                                          * byte that isn't part of it —
                                          * don't lose that keypress      */
                c = (unsigned char)leftover;
                goto redispatch;
            }
            break;
        }
```
*(`shell/linedit.c:395-404`, jumping to the `redispatch:` label at
`:372`, the top of the key `switch`)*

Concretely: press ESC, then Enter, quickly. The Enter byte (`'\r'`,
0x0D) arrives inside the 50 ms window, fails the param-byte range,
and is *returned* — `goto redispatch` feeds it back into the key
switch, and the line **submits**. Without the rescue, your Enter
would have been silently eaten by a half-parsed escape. (And yes —
that is a `goto`, used for state-machine re-entry: re-dispatching an
already-read byte to the top of a decision table. This is the honest
use the structured-programming wars carved out as legitimate; the
loop-and-flag alternative is strictly uglier. One label, one jump,
zero shame.)

---

## 10.3 The redraw loop — one row, never wrapped

Every primitive ends with `le_refresh(l)` — the comment at
`linedit.c:196` says it as policy: *"each leaves the screen
refreshed."* The deep idea is **immediate-mode rendering**: the screen
is treated as a *pure function* of `(prompt, buf, len, pos, cols)`,
recomputed from scratch on every keystroke. No diffing, no damage
tracking — at one ≤4 KB frame per human keystroke, brute force is
correct engineering (R26's linear-scan philosophy, drawn on a screen).

```c
static void le_refresh(le_t *l)
{
    int cols = term_cols(l->ofd);        /* re-query: tracks live resizes */
```
*(`shell/linedit.c:166-168`; `term_cols` asks the kernel via
`ioctl(ofd, TIOCGWINSZ, &ws)` and falls back to 80 for size-less
ptys, `:142-147`. Re-asking every frame is what makes mid-edit window
resizes just… work.)*

### The math — two `while` loops, two invariants

```c
    /* Scroll: drop characters on the left until the cursor column fits... */
    while (pos > 0 && plen + pos >= cols) { start++; len--; pos--; }
    /* ...then clip on the right so the row never reaches the final column
     * (writing into the last column triggers terminal auto-wrap). */
    while (len > pos && plen + len >= cols) len--;
```
*(`shell/linedit.c:174-178`)*

Remember §10.1: `plen + pos` *is* the cursor's screen column. The
first loop slides the visible window rightward — dropping leading
characters (`start++`, a Pip walking into the buffer) — until the
invariant **`plen + pos < cols`** holds: *the cursor is on screen.*
The second clips the tail until **`plen + len < cols`**: *no text
ever touches the final column* — because writing the last cell of a
row triggers the terminal's auto-wrap, the row becomes two rows, and
every column calculation afterward is garbage. The editor's whole
single-row guarantee hangs on that strict inequality.

Worked example — `cols = 20`, prompt `"mysh> "` (`plen = 6`), buffer
holds 19 characters with the cursor at the end (`pos = len = 19`):
the scroll loop runs six times (`6 + 19 = 25 ≥ 20` down to
`6 + 13 = 19 < 20`), leaving `start = buf + 6`, `len = pos = 13`:

```
   buf:    a b c d e f g h i j k l m n o p q r s     (len 19, pos 19)
                       └───────── visible ────────┘
   screen: m y s h >   g h i j k l m n o p q r s ▂
   column: 0 1 2 3 4 5 6 7 8 …                18 19
           └─ prompt ─┘└── start=buf+6, 13 chars ─┘ └ cursor parks at 19:
                                                      ON the last column is
                                                      fine — WRITING it isn't
```

### The frame — assembled once, written once

```c
    char *d = l->draw;
    int   n = 0;

    d[n++] = '\r';                                     /* column 0          */
    memcpy(d + n, l->prompt, (size_t)plen); n += plen; /* prompt            */
    for (int i = 0; i < len; i++) {                    /* visible window;   */
        unsigned char c = (unsigned char)start[i];     /* control bytes     */
        d[n++] = (char)(c < 32 ? ' ' : c);             /* render as spaces  */
    }
    n += sprintf(d + n, "\x1b[0K");                    /* erase stale tail  */
    d[n++] = '\r';
    if (plen + pos > 0)
        n += sprintf(d + n, "\x1b[%dC", plen + pos);   /* park the cursor   */
    write_all(l->ofd, d, (size_t)n);
}
```
*(`shell/linedit.c:180-193`)*

Read the frame as choreography: **`\r`** comes home to column 0 (the
humblest cursor command, and the most portable); the prompt and the
clipped window are laid down; **`ESC[0K`** erases from cursor to
end-of-line — this is what removes the *stale tail* after a Backspace
or Ctrl-K, without touching the rest of the screen; a second **`\r`**
comes home again; and **`ESC[<n>C`** marches the cursor forward
exactly `plen + pos` columns to its parking spot. Three subtleties:

- **Control bytes render as spaces** (`c < 32 ? ' '`): a literal tab
  or stray ESC *in the buffer* would move the cursor unpredictably
  and desynchronize the column↔index correspondence the math depends
  on. One byte, one column — the bijection is sacred. (The header
  admits the known UTF-8 wrinkle at `linedit.c:41-43`: multi-byte
  characters edit correctly but count as multiple columns — honest
  scope.)
- **One `write_all`, one frame**: the pieces are assembled in the
  `draw` scratch buffer (sized `strlen(prompt) + bufsz + 48` at
  `:455` — the 48 is headroom for the two `\r`s and both escapes) and
  hit the terminal in a single write. Piecemeal `printf`s would let
  the user *watch* the row being erased and rebuilt — flicker; one
  atomic frame appears as an instantaneous update.
- **Ctrl-L** (`:414-417`) is the only multi-row escape in the file:
  `\x1b[H\x1b[2J` (home + clear screen), then the same `le_refresh`
  repaints the one row that matters.

---

## 10.4 Cooked fallback intercepts — knowing when *not* to be clever

The final breakdown is about restraint. The editor engages only when
*three* gates all pass — quoted in Chapter 9, re-read now for what
each gate *prevents*:

```c
    /* Raw-mode editing needs a capable tty on both ends: input for keys,
     * stdout for redrawing.  (With stdout redirected, cooked mode is the
     * better experience — the kernel still echoes what you type.) */
    if (isatty(ifd) && isatty(STDOUT_FILENO) && !term_is_dumb()) {
```
*(`shell/linedit.c:451-454`)*

**Gate 1 — input must be a tty.** If stdin is a pipe or file
(`mysh < script`), there are no keystrokes, no arrow keys, no human —
raw mode is meaningless. (In mysh this case rarely even reaches
linedit: `main` only calls `linedit_read` when interactive,
`mysh.c:600-611` — defense in two layers.)

**Gate 2 — *output* must be a tty.** The subtle one. Run interactive
`mysh > build.log` and stdout is a file. The editor's redraw frames —
every `\r`, every `ESC[0K`, every keystroke's full repaint — would be
**written into the log**: kilobytes of cursor-control garbage
polluting a file someone will later read. The fallback keeps the log
byte-clean — and the parenthetical in the comment notes the saved UX:
with cooked mode, *the Gatekeeper still echoes your typing to the
screen* (echo goes to the tty, not to redirected stdout). You can see
what you type *and* the file stays pure.

**Gate 3 — the terminal must speak escape:**

```c
static int term_is_dumb(void)
{
    const char *term = getenv("TERM");
    if (!term) return 0;                 /* unset: assume a real terminal */
    return strcmp(term, "dumb")   == 0 ||
           strcmp(term, "cons25") == 0 ||
           strcmp(term, "emacs")  == 0;
}
```
*(`shell/linedit.c:436-443`)*

A `TERM=dumb` terminal *displays* `ESC[0K` as garbage instead of
erasing — and this is not exotic: Emacs's `M-x shell` sets exactly
`TERM=dumb` (hence its place on the list). The capable-terminal check
is environmental humility.

When any gate fails:

```c
    /* Cooked-mode fallback: scripts, pipes, dumb terminals.  Identical to
     * the shell's historical prompt + fgets behaviour. */
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(buf, (int)bufsz, in)) return -1;
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
```
*(`shell/linedit.c:481-487`)*

Note the degradation *contract*: not a worse editor — the **older
interface**, exactly as the shell behaved before linedit existed
(prompt + `fgets` + §7.1's fence-drop newline chop). Same pattern as
Chapter 6's `@`-in-scripts refusal and Chapter 2's `g_cmd_fd`
fallback: when conditions aren't right, mysh degrades by *choosing a
proven older contract*, never by half-working. That's why your
terminal logic can never pollute a script run: the gates route every
non-interactive byte path around the editor entirely.

---

## 10.5 Lab — read the bursts, race the clock, watch the frames

### Lab 1 — see the bursts with the Gatekeeper's own desk

```sh
cat -v          # cooked mode: the Gatekeeper buffers; -v makes bytes visible
```

Now press Left, Ctrl-Left, Home, F5, then Enter:

```
^[[D^[[1;5D^[[H^[[15~
```

Two lessons in one line: these are the exact bursts §10.2 decodes
(`ESC` prints as `^[`) — *and* this is Chapter 9's opening claim
demonstrated, the cooked Gatekeeper "happily storing them into the
buffer as garbage." Ctrl-D to exit.

### Lab 2 — race the 50 ms clock

At a `mysh>` prompt:

1. Press ESC, wait a beat, then type `x` — the ESC is dropped
   (timeout → lone Escape), the `x` inserts normally.
2. Press ESC then `x` *as fast as you can* (< 50 ms): both vanish —
   the decoder read it as an Alt-x chord and ignored it
   (`linedit.c:351-352`).
3. Type a command, press ESC, then *immediately* Enter: the line
   **submits** — the Enter byte failed the sequence grammar, was
   returned as leftover, and rode `goto redispatch` to the accept
   case. The keystroke-rescue, felt in your fingers.

### Lab 3 — capture the frames

```sh
strace -e trace=write -o /tmp/frames.log ./shell/mysh
mysh> abc        # type slowly, then exit
grep 'write(1' /tmp/frames.log | head -5
```

One `write(1, "\r mysh> a\33[0K\r\33[8C", …)` per keystroke — §10.3's
frame, byte for byte: come home, repaint, erase tail, come home, park
at column `plen + pos`. Now shrink your terminal window mid-edit and
press any key: the new frame is already clipped to the new width —
`term_cols` re-queried, no resize handler needed.

### Lab 4 — the scroll window, measured

Resize a terminal to ~25 columns, then type 40 characters at the
`mysh>` prompt. The display never wraps; the left edge eats
characters as you approach the margin. Press Home — the window snaps
back to the start (`pos = 0` ⇒ the scroll loop never fires). Press
End — it slides again. You are watching the two `while` invariants
(`plen+pos < cols`, `plen+len < cols`) hold in real time.

### Lab 5 — the pollution test (gate 2's payoff)

```sh
./shell/mysh > /tmp/session.log     # interactive, but stdout is a file
echo hi                              # type it — kernel echo shows your keys
exit
cat -v /tmp/session.log              # → "mysh> hi\n…" — NOT ONE ^[ anywhere
```

Then try `TERM=dumb ./shell/mysh` in a normal terminal: plain prompt,
and your arrow keys now insert visible garbage into the line — the
cooked Gatekeeper storing bursts as text, exactly what gate 3 spares
real users from.

---

## 10.6 Chapter 10 — the rules to keep

> **R58.** Track cursors as **indexes** when the numbers serve two
> geometries (`buf + pos` for memmove, `plen + pos` for the screen
> column); use pointers only where you merely walk. Maintain the
> quartet invariant relentlessly: `0 ≤ pos ≤ len < bufsz`,
> `buf[len] == '\0'`, after *every* primitive.
>
> **R59.** One keypress ≠ one byte. Terminals speak a burst-grammar
> (`CSI ::= ESC '[' param* final`, params 0x30–0x3F, final 0x40–0x7E)
> whose only disambiguator from a lone ESC is **time** — a 50 ms
> `poll` window matched to keyboard physics (R38's third timescale).
>
> **R60.** Never swallow what you didn't parse: a malformed sequence's
> terminating byte is somebody's keystroke — return it and
> redispatch (the one honest `goto`). Well-formed-but-unknown
> sequences are the opposite case: consume them *silently*, so F5
> never types `15~`.
>
> **R61.** Render immediate-mode: screen = pure function of
> `(prompt, buf, pos, len, cols)`. Re-query the width every frame;
> assemble the whole frame in a scratch buffer; emit it in **one**
> write. Mask control bytes to spaces — the column↔index bijection is
> what every other calculation stands on.
>
> **R62.** Respect the auto-wrap barrier: keep `plen + pos < cols`
> (scroll left) and `plen + len < cols` (clip right) so text never
> touches the final column. A row that never wraps is a row `\r` can
> always come home to.
>
> **R63.** Gate cleverness on capability *and* destination: both fds
> ttys, `TERM` not dumb — else fall back to the historical contract
> (`fgets`). Editor frames must never land in a redirected file;
> dumb terminals must never be sent CSI. Degrade by choosing the
> older contract, never by half-working.

---

## ⏭ The finale: CHAPTER 11 — *The Life of a Keystroke*

Every chapter built one layer; the finale rides a single command
through all of them, with the entire cast on one stage.

---
---

# CHAPTER 11
# The Life of a Keystroke

> *The synthesis. One command — chosen so that every cast member must
> work it — traced from the first keypress to the next prompt. This
> chapter teaches nothing new; it is short because every line of it is
> a citation to work already done. That is what "understanding your
> own shell" was always going to look like.*

The command:

```
mysh> cat Makefile | tr a-z A-Z | wc -l > count.txt
```

Two residents (`cat`, `wc` — Speedrunners), one stranger (`tr` — the
Mirror), two pipes, one redirect. Curtain up.

### Act I — The Prompt *(Chapters 9–10)*

1. The shell is parked at `mysh>`: the Cooked Gatekeeper is on break
   (`raw_enable`, eight rules crossed out — R53), and the `le_t`
   quartet reads `pos = len = 0`.
2. You press `c`. The tty delivers one raw byte (`VMIN=1`);
   `le_insert` opens a one-byte gap with `memmove` (§1.4), fixes the
   quartet (R58), and one assembled frame — `\r`, prompt, window,
   `ESC[0K`, park — hits the terminal in a single write (R61), text
   never touching the final column (R62). Repeat for every character;
   any arrow keys arrive as 50 ms bursts and are decoded by the CSI
   grammar (R59), with malformed stragglers rescued via
   `goto redispatch` (R60).
3. Enter arrives as `'\r'` — ICRNL is off (§9.2). `le_edit` returns;
   `raw_disable` restores the photocopied rulebook with `TCSADRAIN`,
   sparing any pasted type-ahead (R55). The Gatekeeper is back at his
   desk — which is precisely what arms Ctrl-C for the children about
   to be born (§9.3).

### Act II — The Sentence *(Chapter 7)*

4. `strcspn` fence-drops the newline (R40, school 1). The first
   non-blank byte is `c`, not `@` — the pipeline grammar is chosen
   (R39).
5. `tok_split` scans once through three modes (R41): eleven words —
   `cat` `Makefile` `|` `tr` `a-z` `A-Z` `|` `wc` `-l` `>`
   `count.txt` — each measured and duplicated onto the heap by
   `push_word`, `strndup` spelled out (R40, school 2). No `$` to
   inject today (R42).
6. `parse_pipeline` deals the words into three `stage_t` buckets:
   `|` seals each argv with its NULL fence, `>` steals `count.txt`
   into `stage[2].redir_out` with `append = 0` (R44). The buckets
   borrow every Pip from the token shelf — views, not owners (R43).

### Act III — The Cast Assembles *(Chapters 2, 4, 5, 8)*

7. **Stage 0, `cat` (resident).** `reg_find("cat")` walks the card
   catalog to `&cmd_cat_spec` (R26). A safe `dup` of fd 0, pipe A
   born with both ends stamped `FD_CLOEXEC` (R50), a heap packet
   (R7), two `fdopen`'d private streams, and `pthread_create` hires
   Speedrunner T1 with the machine-Pip-plus-luggage pair (R24). T1
   fires `a->spec->run(...)` — one load at offset 24, one indirect
   call (R23).
8. **Stage 1, `tr` (stranger).** `reg_find` returns NULL — the fork
   in the road that decides a fork happens (§4.3). Pipe B is born,
   stamped. The Mirror flashes (R8); the clone customizes itself in
   the gap — signals to `SIG_DFL`, `close(g_cmd_fd)` (R14), `dup2`
   landing A.read on slot 0 and B.write on slot 1 as *unstamped*
   copies (R47) — then `execvp` performs the costume change: every
   stamped card burns, leaving a clean box (R50); `/usr/bin/tr`
   wakes knowing only the treaty slots (R48).
9. **Stage 2, `wc` (resident).** The redirect opens `count.txt` with
   `O_WRONLY|O_CREAT|O_TRUNC, 0644` (R48 — the permission
   fingerprint), gets stamped, gets `fdopen`'d; T2 is hired. All
   three stages have now run `arg_parse` concurrently — three private
   argtable instances (R27) scanning under three TLS-private
   `optind`s (R32). Zero races, zero mutexes (R33).

### Act IV — The Bytes Flow *(Chapters 1, 3, 5, 8)*

10. `cat`'s `getline` rents and geometrically regrows its heap line
    buffer (R16), `fwrite`s each line into its `FILE`; bytes cross
    pipe A; `tr` uppercases them; pipe B; `wc` counts them into a
    `FILE` whose card leads to `count.txt`.
11. `cat` finishes: `fclose(a->out)` — flush, then the EOF whistle
    (R29). The cascade crosses rooms: `tr` reads EOF on slot 0, exits;
    the kernel closes its slot 1 — the *last* write card on pipe B
    (R49) — and T2's `getline` returns −1. `wc` writes one number,
    settles its ledgers (R19), and whistles in turn.

### Act V — The Settlement *(Chapters 2, 3)*

12. `run_pipeline` joins T1 and T2 — the joins that make every
    borrowed lease legal (R7) — frees the packets, then `waitpid`
    collects `tr`'s ghost (R11). The final stage's status becomes the
    pipeline's.
13. `tok_free` returns all eleven word-boxes to the Clerk, NULLing
    each Pip (R18, R20). `last_status = 0` rides the circuit; the
    prompt prints plain `mysh>` — no `[N]` confession today (§2.3).
14. The loop reaps any stray background ghosts (`WNOHANG`, R11), the
    Gatekeeper steps off his desk again, a fresh quartet zeroes, one
    frame draws — and the shell blocks on `read(0, …, 1)`, waiting
    for *your* next byte.

**Curtain call.** Pip carried every address; the Mirror flashed once
and a stranger changed costume; two Speedrunners shared the room with
private notepads; the Clerk rented every box and took every key back;
the Gatekeeper stood aside and returned on cue. (The Tin-Can Timers
rest this scene — their full trace was §6.4's `@`, the same machinery
wearing a network.) Sixty-three rules, and a single ordinary command
exercises nearly all of them. Remove any chapter and this command
doesn't work; with all of them, it's one line at a prompt.

---
---

# APPENDIX A
# The Cast Character Glossary

*Reference-ready: each character, the concept they embody, the
mechanical truths they carry, and where they live in the book.*

### Pip the Pointer
**Embodies:** the pointer — and every discipline built on addresses.
**Mechanical truths:** an 8-byte box holding an address; `&` reads a
box's number, `*` walks the string; declaration mirrors use; Pip-on-Pip
(`**`) is the out-parameter and `argv`; arrays decay and forget their
size; machine Pips jump into `.text` (`spec->run`, `qsort_r`
callbacks); lifetimes decide everything — down the tower always, up
never, sideways only via the heap; indexes ("house numbers") beat
pointers when the number serves two geometries.
**Home:** Chapters 1, 3, 4, 7, 10 · **Rules:** R1–R7, R15–R27,
R40–R45, R58.

### The Forking Mirror
**Embodies:** `fork()` — process duplication and process *isolation*.
**Mechanical truths:** one flash, two rooms — address space, fd
card-box, stdio furniture, all copied (lazily, COW); returns twice and
`if (pid == 0)` asks which side you're on; the gap between flash and
costume change (`execvp`) is where the clone customizes itself
(signals, fds, redirects); `SIG_IGN` survives exec, handlers don't;
clones leave ghosts until `waitpid` collects them; isolation is why
`cd` can never be delegated and why a clone's crash is survivable —
and Machine 3 (fork *without* exec) is why `g_cmd_fd` needs manual
closes.
**Home:** Chapters 2, 8 · **Rules:** R8–R14, R46–R51.

### The Threaddy Speedrunners
**Embodies:** POSIX threads — concurrency inside one address space.
**Mechanical truths:** same room, own note-pad (stack), own TLS
belt-pouch, own `errno` — *everything else shared*, including the live
fd card-box; cheap and dangerous for the same reason (no walls); a
data race is two workers, one box, one writer, no ordering — its
symptom is "almost always works"; the cures, in order: partition
ownership (per-stage packets, streams, per-call tables), make hidden
library state `__thread`, and only then reach for a lock (mysh ships
zero); `fclose` is the EOF whistle; `pthread_join` is what makes every
borrowed Pip's lease legal.
**Home:** Chapters 5, 8 · **Rules:** R28–R33.

### The Tin-Can Socket Timers
**Embodies:** TCP client networking under strict deadlines.
**Mechanical truths:** a socket is a card to a telephone can; the
connection *is* the 4-tuple (src IP, src port, dst IP, dst port); TCP
is a byte stream — framing (`'\n'`) is the application's job, so
`send` loops and `recv` loops; never wait unbounded — `O_NONBLOCK` +
`EINPROGRESS` + `poll(timeout)` fuses the connect, `SO_RCVTIMEO` fuses
the silence, and after `poll` says writable, `getsockopt(SO_ERROR)`
holds the real verdict; in an in-process, SIGINT-ignoring shell, the
five-second fuses are survival, not politeness.
**Home:** Chapter 6 · **Rules:** R34–R39.

### Supporting cast
**The Warehouse Clerk** *(glibc's allocator — Chapter 3, R15–R21)*:
rents boxes, keeps the ledger entry just before each one, never erases
ghost furniture; `realloc`'s fine print — old box intact on failure —
is why the pointer proxy exists.
**The Cooked Gatekeeper** *(the tty line discipline — Chapters 9–10,
R52–R57, R63)*: echoes, buffers, and edits at his desk, converts
Ctrl-C into an alarm; raw mode is a formal transfer of his duties to
you — photocopied rulebook, minimal window, structural restore, blind
`reset⏎` when all else fails.

---
---

# APPENDIX B
# The System Call and libc Primitive Index

*Alphabetized (leading underscores ignored). "Duty" is the one-line
operational truth; "Deployed in" lists the verified sites in this
repository; "Ch." points to the chapter that teaches it. Entries
marked † are GNU extensions; ‡ entered POSIX in 2008.*

| Primitive | Operational duty | Deployed in | Ch. |
|---|---|---|---|
| `atexit` | register a machine to run at every `exit()` — no context slot, so its state lives in globals | `shell/linedit.c` | 4, 9 |
| `chdir` | move *this* room's working directory — the call that proves `cd` must be a built-in | `shell/mysh.c` | 2 |
| `close` | return a card to the card-box; on a pipe's last write card, deliver EOF | `shell/mysh.c`, `apps/fetch/cmd_fetch.c` | 2, 8 |
| `connect` | tie the local can to a remote 4-tuple endpoint; non-blocking → `EINPROGRESS` receipt | `apps/fetch/cmd_fetch.c` | 6 |
| `dup` | mint a second card to the same bookmark (shared offset) — endpoint safety for thread stages; `g_cmd_fd`'s birth | `shell/mysh.c` | 2, 5, 8 |
| `dup2` | destroy slot *dst*, install a copy of *src*'s card there — the redirect overwrite; copies are born unstamped | `shell/mysh.c` | 8 |
| `execvp` | the costume change: demolish the room's interior, rebuild from a `$PATH`-found binary; keeps PID, cards, cwd; never returns on success | `shell/mysh.c` | 2 |
| `_exit` | leave the room *without* atexit handlers or stdio flushing — the only safe death for a half-built clone | `shell/mysh.c` | 2, 9 |
| `fcntl` | flip flags at two layers: `F_SETFD` = card (`FD_CLOEXEC`), `F_SETFL` = bookmark (`O_NONBLOCK`) | `shell/mysh.c`, `apps/fetch/cmd_fetch.c` | 6, 8 |
| `fclose` | settle a stream: flush the furniture, return the card — the Speedrunners' EOF whistle | `shell/mysh.c`, `apps/*/cmd_*.c` | 5 |
| `fdopen` | wrap a raw card in private stdio furniture — how pipe fds become each thread's `FILE *in/out` | `shell/mysh.c` | 5, 8 |
| `fflush` | force the furniture's bytes out — mandatory before `fork` (double-mail) and before raw `write()`s | `shell/mysh.c`, `shell/linedit.c` | 2, 9 |
| `fgets` | cooked-mode line read — the historical contract every fallback degrades to | `shell/mysh.c`, `shell/linedit.c` | 2, 10 |
| `fopen` | open a file as a stream (creates at 0666 & ~umask — the 0664 fingerprint); the *in-process* redirect plumbing | `shell/mysh.c`, `apps/cat/cmd_cat.c`, `apps/sort/cmd_sort.c` | 8 |
| `fork` | the Mirror's flash: duplicate the whole room (COW), return twice — child gets 0 | `shell/mysh.c` (3 Machines) | 2 |
| `fread` | bulk byte read through a stream — `cat --json`'s 4 KB shovel | `apps/cat/cmd_cat.c` | 3 |
| `free` | return a key to the Clerk — exactly the issued key, exactly once; `free(NULL)` is a no-op | throughout (`shell/tok.c`, `shell/mysh.c`, `apps/*`) | 3 |
| `freeaddrinfo` | return the phone book the resolver built | `apps/fetch/cmd_fetch.c` | 6 |
| `fwrite` | bulk byte write through a stream | `apps/cat/cmd_cat.c`, `apps/sort/cmd_sort.c`, `apps/fetch/cmd_fetch.c` | 3 |
| `gai_strerror` | translate `getaddrinfo`'s *own* error codes (not `errno`) into prose | `apps/fetch/cmd_fetch.c` | 6 |
| `getaddrinfo` | name → linked Pip-chain of candidate endpoints (IPv4+IPv6), filtered by `hints` | `apps/fetch/cmd_fetch.c` | 6 |
| `getcwd` | read this room's working directory — seeds `OLDPWD`, backs `cd -` | `shell/mysh.c` | 2 |
| `getenv` | look up a name on the environment shelf | `shell/mysh.c`, `shell/tok.c`, `shell/linedit.c` | 7, 10 |
| `getline` ‡ | read one line into a buffer it rents and regrows for you (`char **` — it may re-tie your Pip); caller frees | `apps/cat/cmd_cat.c`, `apps/sort/cmd_sort.c` | 1, 3 |
| `getsockopt` | read socket state — `SO_ERROR` holds the connect verdict `poll` can't tell you | `apps/fetch/cmd_fetch.c` | 6 |
| `ioctl` | device-specific control — `TIOCGWINSZ` asks the tty its width, every frame | `shell/linedit.c` | 10 |
| `isatty` | "is this card a terminal?" — the gate between raw UX and the cooked fallback | `shell/mysh.c`, `shell/linedit.c` | 2, 10 |
| `malloc` | rent a box of n bytes from the Clerk; contents uninitialized | `shell/mysh.c`, `shell/tok.c`, `shell/linedit.c`, `apps/*` | 3 |
| `memchr` | find a byte in a box — fetch's frame-mark (`'\n'`) detector | `apps/fetch/cmd_fetch.c` | 6 |
| `memcpy` | copy bytes between non-overlapping boxes | `shell/linedit.c`, `apps/cat`, `apps/sort`, `apps/fetch` | 1, 3 |
| `memmove` | copy bytes even when source and destination overlap — every editor insert/delete/kill | `shell/linedit.c` | 1, 10 |
| `memset` | fill a box with one byte — zeroing fresh buckets (`sizeof *cur`) and `hints` | `shell/mysh.c`, `apps/fetch/cmd_fetch.c` | 1, 6 |
| `open` | rent a card to a file: `O_RDONLY` / `O_WRONLY\|O_CREAT` + `O_TRUNC` vs `O_APPEND`, mode 0644 — the *fork-side* redirect plumbing | `shell/mysh.c` | 8 |
| `pipe` | mint a conveyor belt: two cards, one kernel buffer; reader EOFs when the last write card dies | `shell/mysh.c` | 5, 8 |
| `poll` | "wake me when these fds have events — or when the fuse burns out": 50 ms for escape bursts, 5000 ms for connects | `shell/linedit.c`, `apps/fetch/cmd_fetch.c` | 6, 10 |
| `pthread_create` | hire a Speedrunner into this room: a machine Pip plus a `void *` of luggage | `shell/mysh.c` | 5 |
| `pthread_join` | wait for a Speedrunner to finish — the barrier that legalizes every borrowed lease | `shell/mysh.c` | 5 |
| `qsort_r` † | generic sort over blank-tag elements; your comparator + your `void *ctx` — the C closure | `apps/sort/cmd_sort.c` | 1, 4 |
| `read` | pull raw bytes through a card — one keystroke at a time under `VMIN=1` | `shell/linedit.c` | 9, 10 |
| `realloc` | ask the Clerk for a bigger box (may move; may extend); **on failure the old box survives under the old key** — proxy first | `apps/cat/cmd_cat.c`, `apps/sort/cmd_sort.c`, `apps/fetch/cmd_fetch.c` | 3 |
| `recv` | read bytes from a can — chunks at the stream's whim; 0 = polite hangup; `EAGAIN` = the fuse | `apps/fetch/cmd_fetch.c` | 6 |
| `send` | queue bytes into a can — may take only some; loop (`send_all`) | `apps/fetch/cmd_fetch.c` | 6 |
| `setenv` | write a name onto the environment shelf — `VAR=value`, `OLDPWD`, the `~/.mysh/bin` PATH prepend | `shell/mysh.c` | 2 |
| `setsockopt` | arm socket behavior — `SO_RCVTIMEO`, the silence fuse | `apps/fetch/cmd_fetch.c` | 6 |
| `signal` | set a signal disposition: shell `SIG_IGN`s INT/PIPE; every clone restores `SIG_DFL` before exec | `shell/mysh.c` | 2 |
| `snprintf` | bounded formatting into a caller's box — prompts, port strings, `$?` digits | `shell/mysh.c`, `shell/tok.c`, `apps/fetch/cmd_fetch.c` | 2, 7 |
| `socket` | mint a telephone can in the card-box, family/type from the phone book's candidate | `apps/fetch/cmd_fetch.c` | 6 |
| `stat` | ask the filesystem about a path — does `~/.mysh/bin` exist and is it a directory? | `shell/mysh.c` | 2 |
| `strchr` / `strcspn` | find a byte / measure a span — `=` in assignments, the newline chop, field separators | `shell/mysh.c`, `shell/linedit.c`, `apps/sort/cmd_sort.c` | 7 |
| `strcmp` | compare strings — the registry's whole lookup algorithm | `shell/mysh.c`, `shell/linedit.c`, `apps/*` | 4 |
| `strerror` | translate `errno` into prose for every honest error message | `shell/mysh.c`, `apps/*` | 2–8 |
| `strndup` ‡ | measure-and-duplicate slicing: `malloc(n+1)` + copy + fence (hand-rolled as `push_word`) | `apps/sort/cmd_sort.c` (by hand: `shell/tok.c`) | 7 |
| `strtod` | parse a number for `sort -n` — reads as far as digits allow | `apps/sort/cmd_sort.c` | 1 |
| `tcgetattr` / `tcsetattr` | photocopy / apply the Gatekeeper's rulebook; `TCSADRAIN` drains output, keeps type-ahead | `shell/linedit.c` | 9 |
| `__thread` † | (keyword) re-home a global into per-thread storage — `.tdata` template, `%fs`-relative access; the TLS cure | `vendor/argtable3/argtable3.c` | 5 |
| `waitpid` | collect a clone's ghost and its bit-packed status (`WIFEXITED`/`WEXITSTATUS`); `WNOHANG` to sweep without blocking | `shell/mysh.c` | 2 |
| `write` | push raw bytes through a card — the editor's single-frame emitter (looped as `write_all`) | `shell/linedit.c` | 10 |

*Layering note: several rows are libc machinery over deeper kernel
primitives — `fork` rides `clone`, `pthread_create` rides `clone` +
futexes, `signal` rides `sigaction`, `tcgetattr` rides `ioctl`,
`getaddrinfo` is a whole resolver library, and all of stdio
(`fopen`/`fgets`/`fflush`…) is furniture over `open`/`read`/`write`.
The vendored argtable3 API (`arg_parse`, `arg_print_*`,
`arg_freetable`) is documented in Chapter 4.*

---
---

# COLOPHON

**The book is complete.** Eleven chapters, two appendices, sixty-three
rules, a six-character cast — built one reader-steered chapter at a
time, every code quote extracted verbatim and cited, every testable
claim run against the living shell before it was written down.

**The findings ledger** — real defects this book discovered in its own
subject, by reading and verifying:

| Finding | Where | Status |
|---|---|---|
| `realloc` trap: `buf = realloc(buf, cap)` leaks the old box on OOM | `apps/cat/cmd_cat.c:98` | §3.4 — ✅ **fixed**: proxy applied (`bigger`), old key freed on OOM |
| Trailing-pipe segfault: `ls \|` ships an empty bucket → `reg_find(NULL)` → SIGSEGV, exit 139 (verified) | `shell/mysh.c` (`parse_pipeline`) | §7.5 — ✅ **fixed**: both guards applied; `ls \|`, `\| ls`, `a \| \| b` all re-verified (error + survive, rc 0) |
| Stale doc: `tok.h:13` says "no variable expansion" — double quotes *do* expand (verified) | `shell/tok.h` | §7.2 — ✅ **corrected**: comment now states the real semantics |
| CLOEXEC stamps are load-bearing: removing two `fcntl` lines deadlocks mixed pipelines (verified: 5.002 s vs 0.003 s) | `shell/mysh.c:327-328` | §8.4 — working as designed; do not remove |
| `atexit` net has a hole: signal death mid-edit wrecks the terminal | `shell/linedit.c` | §9.4; handler upgrade in §9.5 Lab 4 |

*Citation edition: every `file:line` reference in Chapters 1–11 was
taken from the tree as of commit `ece1997` — the state the book was
written (and verified) against. The three ledger fixes above landed
after that commit and shift later line numbers slightly
(`shell/mysh.c` +8 below `parse_pipeline`, `apps/cat/cmd_cat.c` +5
below the growth loop, `shell/tok.h` +1); the quoted code itself is
unchanged everywhere else.*

*Still living: ask any question, and the answer will be written into
the book where it belongs.*
