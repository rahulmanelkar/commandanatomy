#!/usr/bin/env bash
# test_apps.sh — manual test runner for rahulbox apps
#
# Usage (run from repo root):
#   ./test_apps.sh                           run all tests
#   ./test_apps.sh --test 1                  run a single test
#   ./test_apps.sh --test 1 --test 5         run multiple specific tests
#
# Test index:
#   1-2   hello         basic greeting, custom name
#   3-4   ls            directory listing, JSON output
#   5     stat          file metadata
#   6-7   wc            default counts, line-only count
#   8-10  cat           plain, numbered lines, show-ends
#   11-13 echo          basic, escape sequences, suppress newline
#   14-15 head/tail     first N lines, last N lines
#   16-18 grep          match, count, invert
#   19-24 sort          alpha, reverse, numeric, numeric-reverse, unique, field
#   25-26 uniq          adjacent dedup, count occurrences
#   27-28 cut           field extraction, character range
#   29    tee           fanout to stdout and file
#   30-36 pipelines     2- through 6-stage concurrent thread pipelines
#   37-43 BNF shell lab VAR=value assignment, $VAR/$?/${VAR} expansion, & background
#   44-47 fetch         TCP line round-trip, --json schema, refused error, mysh built-in
#   48-49 @ mode        interactive NL prompt forwarded to AI mock, non-interactive skip

set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

MYSH="./shell/mysh"
[[ -x "$MYSH" ]] || { printf "error: %s not found — run 'make' first\n" "$MYSH" >&2; exit 1; }

# ── colours (disabled when stdout is not a tty) ───────────────────────────────
if [[ -t 1 ]]; then
    RST='\033[0m' BLD='\033[1m' DIM='\033[2m'
    GRN='\033[0;32m' RED='\033[0;31m' CYN='\033[0;36m' YLW='\033[1;33m'
else
    RST='' BLD='' DIM='' GRN='' RED='' CYN='' YLW=''
fi

# ── fixtures ──────────────────────────────────────────────────────────────────
F_FRUITS=/tmp/rb_fruits.txt   # 5-line file with repeats, for sort/uniq/grep tests
F_NUMS=/tmp/rb_nums.txt       # 5 unsorted integers, for numeric sort tests
F_CSV=/tmp/rb_csv.txt         # 3-column CSV rows, for field-sort and cut tests
F_TEE=/tmp/rb_tee.txt         # output file used by tee tests

setup() {
    printf "banana\napple\ncherry\nbanana\napple\n" > "$F_FRUITS"
    printf "3\n1\n10\n2\n5\n"                       > "$F_NUMS"
    printf "alice,30\nbob,25\ncarol,35\n"           > "$F_CSV"
}
teardown() { rm -f "$F_FRUITS" "$F_NUMS" "$F_CSV" "$F_TEE"; }

# ── fetch helpers ─────────────────────────────────────────────────────────────
# The fetch tests need a live TCP peer.  start_echo_server spins up a one-shot
# line server in the background that reads a single newline-terminated request
# and replies "echo: <request>".  It sets SERVER_PID for the caller to wait on.
SERVER_PID=0
have_python() { command -v python3 > /dev/null 2>&1; }

start_echo_server() {  # $1 = port
    local port=$1 ready
    ready=$(mktemp /tmp/rb_ready_XXXXXX); rm -f "$ready"
    python3 -c '
import socket, sys
port = int(sys.argv[1]); readyfile = sys.argv[2]
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port)); s.listen(1)
s.settimeout(10)               # never hang the suite if no client arrives
open(readyfile, "w").close()   # signal "listening" to the shell
try:
    c, _ = s.accept()
    data = b""
    while not data.endswith(b"\n"):
        chunk = c.recv(1024)
        if not chunk: break
        data += chunk
    c.sendall(b"echo: " + data)
    c.close()
except Exception:
    pass
finally:
    s.close()
' "$port" "$ready" &
    SERVER_PID=$!
    # Wait until the server has bound+listened (ready file appears), max ~2s.
    local i=0
    while [[ ! -e "$ready" ]] && (( i < 100 )); do sleep 0.02; (( i++ )); done
    rm -f "$ready"
}

skip_no_python() {  # $1 = test num, $2 = desc
    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" "$1" "$2"
    printf "  ${YLW}SKIP${RST}  python3 not available (needed for a test TCP server)\n"
}

# ── counters ──────────────────────────────────────────────────────────────────
N_PASS=0 N_FAIL=0

# ── core runner ───────────────────────────────────────────────────────────────
# run_test NUM DESC EXPECTED CMD [ARGS...]
#
#   EXPECTED  exact expected stdout string; "" to accept any non-error output
#
# Prints: test header, command, PASS/FAIL with output or mismatch detail.
run_test() {
    local num=$1 desc=$2 expected=$3; shift 3

    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" "$num" "$desc"
    printf "  ${DIM}\$ %s${RST}\n" "$*"

    local out code
    out=$("$@" 2>&1) && code=0 || code=$?

    if (( code != 0 )); then
        printf "  ${RED}FAIL${RST}  exit code %d\n" "$code"
        [[ -n "$out" ]] && printf "%s\n" "$out" | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
        return
    fi

    if [[ -n "$expected" && "$out" != "$expected" ]]; then
        printf "  ${RED}FAIL${RST}  output mismatch\n"
        printf "  ${YLW}expected:${RST}\n"; printf "%s\n" "$expected" | sed 's/^/    /'
        printf "  ${YLW}got:${RST}\n";      printf "%s\n" "$out"      | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
        return
    fi

    printf "  ${GRN}PASS${RST}\n"
    if [[ -n "$out" ]]; then
        printf "%s\n" "$out" | sed 's/^/    /'
    elif [[ -n "$expected" ]]; then
        printf "    ${DIM}(no stdout — expected: %s)${RST}\n" "$expected"
    else
        printf "    ${DIM}(no stdout output)${RST}\n"
    fi
    N_PASS=$(( N_PASS + 1 ))
}

# run_mysh NUM DESC EXPECTED "pipeline command string"
#
# Writes the pipeline to a temp script and runs it through mysh.
# Displays the pipeline command (not the mysh invocation) for readability.
run_mysh() {
    local num=$1 desc=$2 expected=$3 pipeline=$4
    local tmp; tmp=$(mktemp /tmp/rb_XXXXXX)
    printf '%s\n' "$pipeline" > "$tmp"

    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" "$num" "$desc"
    printf "  ${DIM}[mysh]\$ %s${RST}\n" "${pipeline//$'\n'/ ; }"

    local out code
    out=$("$MYSH" "$tmp" 2>&1) && code=0 || code=$?
    rm -f "$tmp"

    if (( code != 0 )); then
        printf "  ${RED}FAIL${RST}  exit code %d\n" "$code"
        [[ -n "$out" ]] && printf "%s\n" "$out" | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
        return
    fi

    if [[ -n "$expected" && "$out" != "$expected" ]]; then
        printf "  ${RED}FAIL${RST}  output mismatch\n"
        printf "  ${YLW}expected:${RST}\n"; printf "%s\n" "$expected" | sed 's/^/    /'
        printf "  ${YLW}got:${RST}\n";      printf "%s\n" "$out"      | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
        return
    fi

    printf "  ${GRN}PASS${RST}\n"
    if [[ -n "$out" ]]; then
        printf "%s\n" "$out" | sed 's/^/    /'
    elif [[ -n "$expected" ]]; then
        printf "    ${DIM}(no stdout — expected: %s)${RST}\n" "$expected"
    else
        printf "    ${DIM}(no stdout output)${RST}\n"
    fi
    N_PASS=$(( N_PASS + 1 ))
}

# ── test definitions ──────────────────────────────────────────────────────────

# hello ───────────────────────────────────────────────────────────────────────
t_1() {
    run_test 1 "hello — default greeting" \
        "Hello, World!" \
        ./apps/hello/hello
}
t_2() {
    run_test 2 "hello — custom name (--name Alice)" \
        "Hello, Alice!" \
        ./apps/hello/hello --name Alice
}

# ls ──────────────────────────────────────────────────────────────────────────
t_3() {
    run_test 3 "ls — list directory" \
        "" \
        ./apps/ls/ls apps/hello
}
t_4() {
    run_test 4 "ls --json — JSON output for two paths" \
        "" \
        ./apps/ls/ls --json apps/hello apps/echo
}

# stat ────────────────────────────────────────────────────────────────────────
t_5() {
    run_test 5 "stat — file metadata (permissions, size, timestamps)" \
        "" \
        ./apps/stat/stat apps/hello/cmd_hello.c
}

# wc ──────────────────────────────────────────────────────────────────────────
t_6() {
    run_test 6 "wc — default (lines, words, bytes)" \
        "" \
        ./apps/wc/wc testdata/lorem.txt
}
t_7() {
    run_test 7 "wc -l — line count only" \
        "" \
        ./apps/wc/wc -l testdata/lorem.txt
}

# cat ─────────────────────────────────────────────────────────────────────────
t_8() {
    run_test 8 "cat — print file contents" \
        "" \
        ./apps/cat/cat "$F_FRUITS"
}
t_9() {
    run_test 9 "cat -n — number all output lines" \
        "" \
        ./apps/cat/cat -n "$F_FRUITS"
}
t_10() {
    run_test 10 "cat -E — show \$ at end of each line" \
        "" \
        ./apps/cat/cat -E "$F_FRUITS"
}

# echo ────────────────────────────────────────────────────────────────────────
t_11() {
    run_test 11 "echo — basic output" \
        "hello world" \
        ./apps/echo/echo hello world
}
t_12() {
    run_test 12 "echo -e — interpret backslash escapes" \
        $'line1\nline2' \
        ./apps/echo/echo -e 'line1\nline2'
}
t_13() {
    run_test 13 "echo -n — suppress trailing newline" \
        "no newline" \
        ./apps/echo/echo -n 'no newline'
}

# head / tail ─────────────────────────────────────────────────────────────────
t_14() {
    run_test 14 "head -n 3 — first 3 lines" \
        $'banana\napple\ncherry' \
        ./apps/head/head -n 3 "$F_FRUITS"
}
t_15() {
    run_test 15 "tail -n 2 — last 2 lines" \
        $'banana\napple' \
        ./apps/tail/tail -n 2 "$F_FRUITS"
}

# grep ────────────────────────────────────────────────────────────────────────
t_16() {
    run_test 16 "grep — lines matching pattern" \
        $'apple\napple' \
        ./apps/grep/grep apple "$F_FRUITS"
}
t_17() {
    run_test 17 "grep -c — count of matching lines" \
        "2" \
        ./apps/grep/grep -c apple "$F_FRUITS"
}
t_18() {
    run_test 18 "grep -v — invert match (non-matching lines)" \
        $'banana\ncherry\nbanana' \
        ./apps/grep/grep -v apple "$F_FRUITS"
}

# sort ────────────────────────────────────────────────────────────────────────
t_19() {
    run_test 19 "sort — alphabetic" \
        $'apple\napple\nbanana\nbanana\ncherry' \
        ./apps/sort/sort "$F_FRUITS"
}
t_20() {
    run_test 20 "sort -r — reverse alphabetic" \
        $'cherry\nbanana\nbanana\napple\napple' \
        ./apps/sort/sort -r "$F_FRUITS"
}
t_21() {
    run_test 21 "sort -n — numeric ascending" \
        $'1\n2\n3\n5\n10' \
        ./apps/sort/sort -n "$F_NUMS"
}
t_22() {
    run_test 22 "sort -rn — numeric descending" \
        $'10\n5\n3\n2\n1' \
        ./apps/sort/sort -rn "$F_NUMS"
}
t_23() {
    run_test 23 "sort -u — deduplicate while sorting" \
        $'apple\nbanana\ncherry' \
        ./apps/sort/sort -u "$F_FRUITS"
}
t_24() {
    run_test 24 "sort -k -t — sort CSV by second field (numeric)" \
        $'bob,25\nalice,30\ncarol,35' \
        ./apps/sort/sort -t, -k2 -n "$F_CSV"
}

# uniq ────────────────────────────────────────────────────────────────────────
t_25() {
    # Fruits file has runs of identical lines (not sorted), so only adjacent
    # duplicates are collapsed — matches standard uniq behaviour.
    run_test 25 "uniq — deduplicate adjacent runs" \
        $'banana\napple\ncherry\nbanana\napple' \
        ./apps/uniq/uniq "$F_FRUITS"
}
t_26() {
    # Sort first so all duplicates are adjacent, then count occurrences.
    run_test 26 "uniq -c — count occurrences (piped from sort)" \
        "" \
        bash -c "./apps/sort/sort $F_FRUITS | ./apps/uniq/uniq -c"
}

# cut ─────────────────────────────────────────────────────────────────────────
t_27() {
    run_test 27 "cut -f -d — extract first CSV field" \
        $'alice\nbob\ncarol' \
        ./apps/cut/cut -f1 -d, "$F_CSV"
}
t_28() {
    run_test 28 "cut -c — select first 3 characters per line" \
        $'ban\napp\nche\nban\napp' \
        ./apps/cut/cut -c1-3 "$F_FRUITS"
}

# tee ─────────────────────────────────────────────────────────────────────────
t_29() {
    rm -f "$F_TEE"
    # tee must pass stdout through AND write to the file simultaneously
    run_test 29 "tee — fanout: stdout passthrough + write to file" \
        "hello tee" \
        bash -c "printf 'hello tee\n' | ./apps/tee/tee $F_TEE"

    # Extra file-content check (informational, not counted separately)
    if [[ -f "$F_TEE" ]]; then
        local content; content=$(< "$F_TEE")
        if [[ "$content" == "hello tee" ]]; then
            printf "    ${GRN}file ok:${RST} %s written correctly\n" "$F_TEE"
        else
            printf "    ${RED}file wrong:${RST} %s contains: %s\n" "$F_TEE" "$content"
        fi
    else
        printf "    ${RED}file missing:${RST} %s was not created\n" "$F_TEE"
    fi
}

# pipelines through mysh ──────────────────────────────────────────────────────

t_30() {
    run_mysh 30 "pipe 2-stage: echo | wc -w" \
        "" \
        "echo hello world | wc -w"
}

t_31() {
    run_mysh 31 "pipe 2-stage: cat | grep  (both internal threads)" \
        $'apple\napple' \
        "cat $F_FRUITS | grep apple"
}

t_32() {
    run_mysh 32 "pipe 3-stage: cat | sort | uniq  (3 internal threads)" \
        $'apple\nbanana\ncherry' \
        "cat $F_FRUITS | sort | uniq"
}

t_33() {
    # Verifies sort thread-safety: two concurrent sort threads must not race
    # on comparator state.  Previously broken; fixed by switching to qsort_r.
    run_mysh 33 "pipe 4-stage: cat | sort | uniq -c | sort -rn  (2 concurrent sort threads)" \
        "" \
        "cat $F_FRUITS | sort | uniq -c | sort -rn"
}

t_34() {
    run_mysh 34 "pipe 5-stage: cat | grep | sort | uniq -c | sort -rn" \
        "" \
        "cat $F_FRUITS | grep -v cherry | sort | uniq -c | sort -rn"
}

t_35() {
    # Uses testdata file for a longer input with 5 lorem sentences
    run_mysh 35 "pipe 5-stage: cat | grep | sort | uniq -c | sort -rn | head -n 3" \
        "" \
        "cat testdata/lorem.txt testdata/lorem2.txt testdata/lorem3.txt | grep Lorem | sort | uniq -c | sort -rn | head -n 3"
}

t_36() {
    rm -f "$F_TEE"
    # tee as a middle stage: cat | tee (writes file) | wc -l
    # Verifies that tee's thread correctly fans out while the pipeline continues.
    run_mysh 36 "pipe 3-stage: cat | tee FILE | wc -l  (tee mid-pipeline)" \
        "" \
        "cat $F_FRUITS | tee $F_TEE | wc -l"

    if [[ -f "$F_TEE" ]]; then
        local lines; lines=$(wc -l < "$F_TEE")
        printf "    ${GRN}file ok:${RST} %s has %s line(s)\n" "$F_TEE" "$lines"
    else
        printf "    ${RED}file missing:${RST} tee did not write %s\n" "$F_TEE"
    fi
}

# BNF shell lab: VAR=value, $VAR expansion, background & ──────────────────────

t_37() {
    run_mysh 37 'VAR=value assignment + $VAR expansion' \
        "hello" \
        $'GREETING=hello\necho $GREETING'
}

t_38() {
    run_mysh 38 '${VAR} braced expansion inside double quotes' \
        "Hello world" \
        $'WORD=world\necho "Hello ${WORD}"'
}

t_39() {
    run_mysh 39 '$? is 0 after successful command' \
        $'ok\n0' \
        $'echo ok\necho $?'
}

t_40() {
    run_mysh 40 '$? reflects non-zero exit (grep no match exits 1)' \
        "1" \
        $'grep zzznomatch /dev/null\necho $?'
}

t_41() {
    # Single quotes must suppress $VAR expansion inside mysh.
    local tmp; tmp=$(mktemp /tmp/rb_XXXXXX)
    printf "X=nope\necho '\$X'\n" > "$tmp"

    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" 41 "single-quote suppresses \$VAR expansion"
    printf "  ${DIM}[mysh]\$ X=nope; echo '\$X'${RST}\n"

    local out code
    out=$("$MYSH" "$tmp" 2>&1) && code=0 || code=$?
    rm -f "$tmp"

    if (( code != 0 )); then
        printf "  ${RED}FAIL${RST}  exit code %d\n" "$code"
        N_FAIL=$(( N_FAIL + 1 ))
    elif [[ "$out" == '$X' ]]; then
        printf "  ${GRN}PASS${RST}\n    %s\n" "$out"
        N_PASS=$(( N_PASS + 1 ))
    else
        printf "  ${RED}FAIL${RST}  expected literal '\$X', got: %s\n" "$out"
        N_FAIL=$(( N_FAIL + 1 ))
    fi
}

t_42() {
    # Background jobs must not block the shell.  'sleep 5 &; echo after'
    # should complete in well under 3 seconds and print [bg] PID.
    local tmp; tmp=$(mktemp /tmp/rb_XXXXXX)
    printf 'sleep 5 &\necho after\n' > "$tmp"

    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" 42 "background & does not block the shell"
    printf "  ${DIM}[mysh]\$ sleep 5 &; echo after${RST}\n"

    local out code
    out=$(timeout 3 "$MYSH" "$tmp" 2>&1); code=$?
    rm -f "$tmp"

    if (( code == 124 )); then
        printf "  ${RED}FAIL${RST}  shell blocked — timed out after 3 s\n"
        N_FAIL=$(( N_FAIL + 1 ))
    elif echo "$out" | grep -q '^\[bg\] [0-9]' && echo "$out" | grep -q 'after'; then
        printf "  ${GRN}PASS${RST}\n"
        printf "%s\n" "$out" | sed 's/^/    /'
        N_PASS=$(( N_PASS + 1 ))
    else
        printf "  ${RED}FAIL${RST}  expected '[bg] PID' and 'after' in output\n"
        printf "%s\n" "$out" | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
    fi
}

t_43() {
    # $VAR expansion used as a command argument inside a pipeline.
    local tmp; tmp=$(mktemp /tmp/rb_XXXXXX)
    printf 'PATTERN=apple\ncat %s | grep $PATTERN\n' "$F_FRUITS" > "$tmp"

    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" 43 "\$VAR used as argument inside a pipeline"
    printf "  ${DIM}[mysh]\$ PATTERN=apple; cat %s | grep \$PATTERN${RST}\n" "$F_FRUITS"

    local out code expected
    out=$("$MYSH" "$tmp" 2>&1) && code=0 || code=$?
    rm -f "$tmp"
    expected=$'apple\napple'

    if (( code != 0 )); then
        printf "  ${RED}FAIL${RST}  exit code %d\n" "$code"
        [[ -n "$out" ]] && printf "%s\n" "$out" | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
    elif [[ "$out" == "$expected" ]]; then
        printf "  ${GRN}PASS${RST}\n"
        printf "%s\n" "$out" | sed 's/^/    /'
        N_PASS=$(( N_PASS + 1 ))
    else
        printf "  ${RED}FAIL${RST}  output mismatch\n"
        printf "  ${YLW}expected:${RST}\n"; printf "%s\n" "$expected" | sed 's/^/    /'
        printf "  ${YLW}got:${RST}\n";      printf "%s\n" "$out"      | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
    fi
}

# fetch ───────────────────────────────────────────────────────────────────────

t_44() {
    have_python || { skip_no_python 44 "fetch — TCP line round-trip"; return; }
    start_echo_server 54370
    run_test 44 "fetch — TCP line round-trip (send + reply)" \
        "echo: hello fetch" \
        ./apps/fetch/fetch -H 127.0.0.1 -p 54370 "hello fetch"
    wait "$SERVER_PID" 2>/dev/null
}

t_45() {
    have_python || { skip_no_python 45 "fetch --json — stable schema"; return; }
    start_echo_server 54371
    # Exact JSON match: keys, echoed host/port/sent, byte count, escaped reply.
    # "echo: ping\n" is 11 bytes; the reply newline is escaped as \n.
    run_test 45 "fetch --json — stable schema round-trip" \
        '{"ok": true, "host": "127.0.0.1", "port": 54371, "sent": "ping", "bytes": 11, "reply": "echo: ping\n"}' \
        ./apps/fetch/fetch --json -H 127.0.0.1 -p 54371 ping
    wait "$SERVER_PID" 2>/dev/null
}

t_46() {
    # No server is listening: connect must fail fast, exit non-zero, and print
    # a clear "refused" message to stderr.
    local port=54399
    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" 46 "fetch — connection refused exits non-zero"
    printf "  ${DIM}\$ ./apps/fetch/fetch -H 127.0.0.1 -p %s hi${RST}\n" "$port"

    local out code
    out=$(./apps/fetch/fetch -H 127.0.0.1 -p "$port" hi 2>&1); code=$?

    if (( code != 0 )) && printf '%s' "$out" | grep -qi "refused"; then
        printf "  ${GRN}PASS${RST}\n"; printf "%s\n" "$out" | sed 's/^/    /'
        N_PASS=$(( N_PASS + 1 ))
    else
        printf "  ${RED}FAIL${RST}  expected non-zero exit and a 'refused' message (got exit=%d)\n" "$code"
        printf "%s\n" "$out" | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
    fi
}

t_47() {
    have_python || { skip_no_python 47 "fetch — runs as a mysh built-in"; return; }
    start_echo_server 54372
    # Proves fetch is registered in the shell registry and runs in-process.
    run_mysh 47 "fetch — runs as a mysh built-in" \
        "echo: viamysh" \
        "fetch -H 127.0.0.1 -p 54372 viamysh"
    wait "$SERVER_PID" 2>/dev/null
}

# @ natural-language mode ─────────────────────────────────────────────────────

t_48() {
    have_python || { skip_no_python 48 "@ — interactive NL prompt via fetch"; return; }
    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" 48 "@ — interactive prompt forwarded to AI mock (pty)"
    printf "  ${DIM}[mysh pty]\$ @ list files please${RST}\n"

    # Drive mysh through a real pty (the @ mode is interactive-only) with a
    # mock AI server on port 5001 — the endpoint the shell forwards '@' to.
    # The harness exits 0 iff the AI reply marker appears in the transcript.
    local out code
    out=$(python3 - <<'PY'
import os, pty, socket, threading, time, select, signal, sys

PORT, PROMPT = 5001, b"list files please"
MARKER = b"AI_REPLY:" + PROMPT

ready = threading.Event()

def server():
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind(("127.0.0.1", PORT)); s.listen(1); s.settimeout(10)
        ready.set()                       # bound+listening — safe to connect
        c, _ = s.accept()
        data = b""
        while not data.endswith(b"\n"):
            ch = c.recv(1024)
            if not ch: break
            data += ch
        c.sendall(b"AI_REPLY:" + data.strip() + b"\n")
        c.close()
    except Exception:
        pass
    finally:
        s.close()

t = threading.Thread(target=server); t.start()
if not ready.wait(5):                     # server failed to listen
    sys.exit(2)

pid, fd = pty.fork()
if pid == 0:
    os.execv("./shell/mysh", ["./shell/mysh"])
    os._exit(127)

os.write(fd, b"@ " + PROMPT + b"\n")

# Read until the AI reply marker appears (deterministic), then quit — rather
# than guessing timing with fixed sleeps.
out = b""
deadline = time.time() + 10
while MARKER not in out and time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.3)
    if r:
        try: chunk = os.read(fd, 4096)
        except OSError: break
        if not chunk: break
        out += chunk

os.write(fd, b"exit\n")
time.sleep(0.2)

try: os.kill(pid, signal.SIGKILL)   # bound the test; harmless if already gone
except OSError: pass
try: os.waitpid(pid, 0)
except OSError: pass
t.join()

sys.stdout.write(out.decode(errors="replace"))
sys.exit(0 if MARKER in out else 1)
PY
)
    code=$?

    if (( code == 0 )); then
        printf "  ${GRN}PASS${RST}\n"; printf "%s\n" "$out" | sed 's/^/    /'
        N_PASS=$(( N_PASS + 1 ))
    else
        printf "  ${RED}FAIL${RST}  AI reply marker not found in pty transcript\n"
        printf "%s\n" "$out" | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
    fi
}

t_49() {
    # '@' in non-interactive mode must be skipped gracefully (notice to stderr)
    # without blocking the rest of the script.
    local tmp; tmp=$(mktemp /tmp/rb_XXXXXX)
    printf '@ this should be ignored\necho still-running\n' > "$tmp"

    printf "\n${BLD}Test %-3s${RST}  ${CYN}%s${RST}\n" 49 "@ — skipped gracefully in non-interactive mode"
    printf "  ${DIM}[mysh]\$ @ this should be ignored ; echo still-running${RST}\n"

    local out code
    out=$("$MYSH" "$tmp" 2>/dev/null) && code=0 || code=$?
    rm -f "$tmp"

    if (( code == 0 )) && [[ "$out" == "still-running" ]]; then
        printf "  ${GRN}PASS${RST}\n"; printf "%s\n" "$out" | sed 's/^/    /'
        N_PASS=$(( N_PASS + 1 ))
    else
        printf "  ${RED}FAIL${RST}  expected stdout 'still-running' (got exit=%d)\n" "$code"
        printf "%s\n" "$out" | sed 's/^/    /'
        N_FAIL=$(( N_FAIL + 1 ))
    fi
}

# ── registry & dispatch ───────────────────────────────────────────────────────
ALL_TESTS=(1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49)

# parse --test N flags; multiple --test flags accumulate
SELECTED=()
while (( $# > 0 )); do
    case $1 in
        --test)
            [[ $# -lt 2 ]] && { printf "error: --test requires a number\n" >&2; exit 1; }
            SELECTED+=("$2"); shift 2
            ;;
        *)
            printf "unknown option: %s\n  usage: %s [--test N] ...\n" "$1" "$0" >&2
            exit 1
            ;;
    esac
done
[[ ${#SELECTED[@]} -eq 0 ]] && SELECTED=("${ALL_TESTS[@]}")

# ── run ───────────────────────────────────────────────────────────────────────
printf "${BLD}rahulbox app test suite${RST}\n"
printf "running %d test(s): %s\n" "${#SELECTED[@]}" "${SELECTED[*]}"

setup
for num in "${SELECTED[@]}"; do
    if declare -f "t_${num}" > /dev/null 2>&1; then
        "t_${num}"
    else
        printf "\n${RED}Test %-3s${RST}  not defined\n" "$num"
        N_FAIL=$(( N_FAIL + 1 ))
    fi
done
teardown

# ── summary ───────────────────────────────────────────────────────────────────
total=$(( N_PASS + N_FAIL ))
printf "\n${BLD}─────────────────────────────────────${RST}\n"
printf "${GRN}%d passed${RST}  ${RED}%d failed${RST}  of %d run\n" "$N_PASS" "$N_FAIL" "$total"
(( N_FAIL == 0 )) && exit 0 || exit 1
