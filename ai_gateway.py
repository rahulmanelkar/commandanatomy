#!/usr/bin/env python3
"""
ai_gateway.py — natural-language gateway for the rahulbox shell.

The rahulbox shell forwards any interactive line that begins with '@' as a raw,
newline-terminated prompt over TCP to 127.0.0.1:5001 (it shells out to the
built-in `fetch` app — see apps/fetch/cmd_fetch.c). It then blocks reading
exactly one '\\n'-terminated line of reply before returning to the prompt.

This script is the other end of that socket: it reads one prompt line, asks an
OpenRouter free-tier model how to accomplish it using only rahulbox's own
commands, and writes the answer back as a single clean line.

The set of commands the model is allowed to use — and the options each one
accepts — is loaded at startup from the shell itself via `mysh --commands-json`
(see shell/mysh.c). The system prompt is generated from that catalog, so the
model's known vocabulary never drifts from what the binary can actually run.

Wire protocol (must match apps/fetch/cmd_fetch.c exactly):
  * Request  : <prompt text>\\n        (one line, client closes after reading reply)
  * Response : <single clean line>\\n  (NO internal newlines — fetch prints the
               whole buffer verbatim once it sees the first '\\n')

Latency budget: the shell's fetch aborts with "recv: timed out" after 5 s of
silence (FETCH_RECV_TIMEOUT_SECS). We cannot stream keep-alives (fetch would
print them concatenated with the answer), so the full round-trip must land
inside that window. HTTP_TIMEOUT defaults just under 5 s and a timeout returns a
clean one-line error rather than letting the shell hang.

Dependencies: standard library only (socket, threading, urllib, subprocess). No
`requests`.

Usage:
    export OPENROUTER_API_KEY=sk-or-...
    python3 ai_gateway.py
"""

import json
import os
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request

# ── configuration ────────────────────────────────────────────────────────────
HOST = os.environ.get("AI_GATEWAY_HOST", "127.0.0.1")
PORT = int(os.environ.get("AI_GATEWAY_PORT", "5001"))

# Path to the compiled rahulbox shell. Its `--commands-json` boot flag prints the
# command catalog used to build the system prompt below, so the model's known
# vocabulary always matches what the shell can actually run. Defaults to the
# sibling shell/mysh next to this script; override if the binary lives elsewhere.
_HERE = os.path.dirname(os.path.abspath(__file__))
MYSH_BIN = os.environ.get("AI_GATEWAY_MYSH", os.path.join(_HERE, "shell", "mysh"))

OPENROUTER_URL = "https://openrouter.ai/api/v1/chat/completions"

# Default engine. Now that the OpenRouter account is funded, we use the paid
# "google/gemini-2.5-flash" — fast and reliable, comfortably within the shell's
# 5s fetch budget, and not subject to the free tier's churn/rate-limiting.
# Override with AI_GATEWAY_MODEL (e.g. a ":free" model for zero-cost testing).
MODEL = os.environ.get("AI_GATEWAY_MODEL", "google/gemini-2.5-flash")

API_KEY = os.environ.get("OPENROUTER_API_KEY")

# Keep this just under the shell's 5 s fetch recv timeout so a slow upstream
# yields a graceful one-line error instead of the shell's own timeout message.
HTTP_TIMEOUT = float(os.environ.get("AI_GATEWAY_TIMEOUT", "4.5"))

# Guard rails on the socket side.
MAX_REQUEST_BYTES = 8192      # cap a single prompt line
READ_TIMEOUT_SECS = 30        # don't let a silent client pin a worker thread
MAX_REPLY_CHARS = 2000        # the shell shows this inline; keep it sane

# ── system prompt: sandbox the model to the rahulbox command set ─────────────
#
# The behavioural rules below are fixed policy. The command *vocabulary* (rule 1)
# and the COMMAND REFERENCE section are filled in at startup from the live shell
# catalog — `mysh --commands-json` — by init_system_prompt(). Adding a command
# to the shell therefore teaches the gateway about it automatically; there is no
# hand-maintained command list to keep in sync here.
#
# FALLBACK_APPS is used ONLY when the catalog cannot be loaded (e.g. mysh is not
# built yet) so the gateway still starts, degraded, instead of failing outright.
FALLBACK_APPS = ("hello", "ls", "stat", "wc", "cat", "echo", "head", "tail",
                 "grep", "sort", "uniq", "cut", "tee", "pkg", "fetch")

RULES_TEMPLATE = (
    'You are a deterministic command-translation engine for "rahulbox", a '
    "small custom Unix-style shell written in C. Your only job is to translate "
    "the user's natural-language request into exactly ONE valid rahulbox "
    "command line. You are NOT a chat assistant and you do not converse.\n"
    "\n"
    "ABSOLUTE RULES — never break these:\n"
    "\n"
    "1. EXCLUSIVE VOCABULARY. Use ONLY these {count} built-in rahulbox commands, "
    "and for each only the flags and options written for it in the COMMAND "
    "REFERENCE below: {vocab}. Treat every other program name as nonexistent. "
    "NEVER invent, assume, or borrow a command, flag, option, or syntax that is "
    "not present in that reference.\n"
    "\n"
    "2. NO COREUTILS. rahulbox does NOT ship cp, mv, rm, find, awk, sed, xargs, "
    "chmod, or any utility absent from the list above — never emit or mention "
    "one. If the request cannot be satisfied with the listed tools, or is "
    "unrelated to operating the shell, emit a single echo command that says so, "
    'e.g.: echo "rahulbox has no tool for that".\n'
    "\n"
    '3. SYNTAX. The only operator is the pipe "|", connecting commands left to '
    'right. NEVER use "&&", "||", ";", subshells, backticks, "$(...)", '
    "redirection you were not explicitly asked for, or any other scripting "
    "construct.\n"
    "\n"
    "4. OUTPUT FORMAT (critical — the shell executes your reply verbatim). Emit "
    "ONLY the raw command line: the exact characters to run, on ONE line, and "
    "nothing else. NO markdown, NO code fences, NO backticks, NO leading or "
    "trailing prose, NO explanation, NO commentary, and NO dash-appended "
    'clause. For example, for "show the TODO lines in notes.txt" you reply with '
    "exactly:\n"
    "cat notes.txt | grep TODO\n"
    "and not one character more.\n"
    "\n"
    "COMMAND REFERENCE — the only commands you may use, each with its summary "
    "and the options it accepts (indented). Honour these exact names and "
    "flags:\n"
    "\n"
    "{reference}"
)

# Built once at startup by init_system_prompt(); read by query_openrouter().
SYSTEM_PROMPT = None


def load_catalog(mysh_bin):
    """Return the shell's command catalog (list of dicts) or None on any error.

    Runs `mysh --commands-json` and parses its JSON array. Never raises: a
    missing binary, non-zero exit, or unparseable output is logged and yields
    None so the caller can fall back to FALLBACK_APPS.
    """
    try:
        proc = subprocess.run([mysh_bin, "--commands-json"],
                              capture_output=True, timeout=10)
    except FileNotFoundError:
        log("catalog: mysh not found at %s" % mysh_bin)
        return None
    except subprocess.TimeoutExpired:
        log("catalog: `%s --commands-json` timed out" % mysh_bin)
        return None
    except OSError as e:
        log("catalog: cannot run %s — %s" % (mysh_bin, e))
        return None

    if proc.returncode != 0:
        log("catalog: %s exited with status %d" % (mysh_bin, proc.returncode))
        return None
    try:
        data = json.loads(proc.stdout.decode("utf-8", errors="replace"))
    except json.JSONDecodeError as e:
        log("catalog: unparseable JSON from mysh — %s" % e)
        return None
    if not isinstance(data, list) or not data:
        log("catalog: mysh returned an empty or non-array catalog")
        return None
    return data


def _render_reference(catalog):
    """Render the catalog as compact per-command help for the system prompt.

    One block per command: a "name — summary" header followed by its usage lines
    indented four spaces. The bare "Options:" header line is dropped as noise.
    """
    blocks = []
    for cmd in catalog:
        name = cmd.get("name")
        if not name:
            continue
        summary = cmd.get("summary", "")
        head = "%s — %s" % (name, summary) if summary else name
        usage = [u for u in cmd.get("usage", [])
                 if u.strip() and u.strip() != "Options:"]
        blocks.append(head + "".join("\n    " + u for u in usage))
    return "\n".join(blocks)


def build_system_prompt(catalog):
    """Fill RULES_TEMPLATE from the catalog, or FALLBACK_APPS if it is None."""
    if catalog:
        names = [c["name"] for c in catalog if c.get("name")]
        reference = _render_reference(catalog)
    else:
        names = list(FALLBACK_APPS)
        reference = "\n".join("- %s" % n for n in names)
    return RULES_TEMPLATE.format(count=len(names),
                                 vocab=", ".join(names),
                                 reference=reference)


def init_system_prompt():
    """Load the shell catalog and build SYSTEM_PROMPT once, at startup."""
    global SYSTEM_PROMPT
    catalog = load_catalog(MYSH_BIN)
    SYSTEM_PROMPT = build_system_prompt(catalog)
    if catalog:
        log("catalog: loaded %d commands from %s" % (len(catalog), MYSH_BIN))
    else:
        log("catalog: load failed; using built-in fallback vocabulary (%d apps)"
            % len(FALLBACK_APPS))


# ── logging ──────────────────────────────────────────────────────────────────
def log(msg):
    """Timestamped line on stderr (stdout is reserved for nothing here)."""
    sys.stderr.write("[ai_gateway] %s\n" % msg)
    sys.stderr.flush()


# ── single-line sanitiser (enforces the wire protocol) ───────────────────────
def to_single_line(text):
    """Collapse all whitespace/newlines into single spaces and bound length.

    fetch prints the reply buffer verbatim, so any stray '\\n' would corrupt the
    shell's display and confuse its line framing. str.split() with no argument
    splits on every run of whitespace (spaces, tabs, '\\r', '\\n'), so joining
    with single spaces guarantees exactly one clean line.
    """
    collapsed = " ".join((text or "").split())
    if not collapsed:
        collapsed = "(rahulbox AI returned an empty response)"
    if len(collapsed) > MAX_REPLY_CHARS:
        collapsed = collapsed[:MAX_REPLY_CHARS - 1].rstrip() + "…"
    return collapsed


# ── OpenRouter call (urllib only, no third-party HTTP) ───────────────────────
def query_openrouter(prompt):
    """POST the chat completion and return the assistant's text (one string).

    Never raises to the caller for expected network/HTTP/JSON problems — every
    failure mode resolves to a human-readable one-line string so the shell
    always gets its single line of reply.
    """
    body = json.dumps({
        "model": MODEL,
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": prompt},
        ],
        "temperature": 0,      # deterministic: same request -> same command
        "max_tokens": 300,
    }).encode("utf-8")

    req = urllib.request.Request(
        OPENROUTER_URL,
        data=body,
        method="POST",
        headers={
            "Authorization": "Bearer %s" % API_KEY,
            "Content-Type": "application/json",
            # Optional OpenRouter attribution headers (harmless if ignored).
            "HTTP-Referer": "https://github.com/rahulbox/rahulbox",
            "X-Title": "rahulbox shell",
        },
    )

    try:
        with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT) as resp:
            payload = json.loads(resp.read().decode("utf-8", errors="replace"))
    except urllib.error.HTTPError as e:
        return _http_error_message(e)
    except urllib.error.URLError as e:
        reason = getattr(e, "reason", e)
        if isinstance(reason, socket.timeout):
            return ("rahulbox AI timed out after %.1fs waiting for OpenRouter "
                    "(the shell aborts at 5s)." % HTTP_TIMEOUT)
        return "rahulbox AI could not reach OpenRouter: %s." % reason
    except (socket.timeout, TimeoutError):
        return ("rahulbox AI timed out after %.1fs waiting for OpenRouter "
                "(the shell aborts at 5s)." % HTTP_TIMEOUT)
    except json.JSONDecodeError:
        return "rahulbox AI got an unparseable response from OpenRouter."

    # Successful HTTP, inspect the JSON body.
    choices = payload.get("choices") or []
    if choices:
        message = choices[0].get("message") or {}
        content = message.get("content")
        if content:
            return content

    err = payload.get("error")
    if isinstance(err, dict) and err.get("message"):
        return "rahulbox AI error: %s" % err["message"]

    return "rahulbox AI error: OpenRouter returned no completion."


def _http_error_message(e):
    """Turn an HTTPError into a one-line, OpenRouter-aware message."""
    detail = ""
    try:
        data = json.loads(e.read().decode("utf-8", errors="replace"))
        detail = ((data.get("error") or {}).get("message")) or ""
    except Exception:
        detail = ""
    if e.code in (401, 403):
        return ("rahulbox AI was rejected by OpenRouter (HTTP %d): check "
                "OPENROUTER_API_KEY%s." % (e.code, (" — " + detail) if detail else ""))
    if e.code == 429:
        return ("rahulbox AI hit the free-tier rate limit (HTTP 429); wait a "
                "moment and try again%s." % ((" — " + detail) if detail else ""))
    if detail:
        return "rahulbox AI error (HTTP %d): %s" % (e.code, detail)
    return "rahulbox AI error: OpenRouter returned HTTP %d." % e.code


# ── request handling ─────────────────────────────────────────────────────────
def answer(prompt):
    """Map a raw prompt to the reply text (still possibly multi-line here)."""
    if not API_KEY:
        return ("rahulbox AI is unconfigured: set OPENROUTER_API_KEY in the "
                "gateway's environment and restart ai_gateway.py.")
    prompt = prompt.strip()
    if not prompt:
        return "rahulbox AI got an empty prompt; type a request after '@'."
    return query_openrouter(prompt)


def read_request_line(conn):
    """Read bytes until the first '\\n' (or EOF / size cap); return the prompt.

    The shell sends the prompt followed by exactly one newline, so this returns
    as soon as that newline arrives.
    """
    buf = bytearray()
    while b"\n" not in buf and len(buf) < MAX_REQUEST_BYTES:
        chunk = conn.recv(4096)
        if not chunk:
            break                       # client closed before sending newline
        buf.extend(chunk)
    nl = buf.find(b"\n")
    if nl != -1:
        buf = buf[:nl]
    return buf.decode("utf-8", errors="replace").rstrip("\r")


def handle_conn(conn, addr):
    """One prompt in, one '\\n'-terminated line out, then close."""
    started = time.time()
    reply = "rahulbox AI gateway error: internal failure."
    try:
        conn.settimeout(READ_TIMEOUT_SECS)
        prompt = read_request_line(conn)
        log("prompt from %s: %r" % (addr[0], prompt[:120]))
        reply = to_single_line(answer(prompt))
    except Exception as e:                          # never let a worker die silently
        reply = to_single_line("rahulbox AI gateway error: %s" % e)
    finally:
        try:
            conn.sendall((reply + "\n").encode("utf-8"))
        except Exception as e:
            log("failed to send reply to %s: %s" % (addr[0], e))
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except Exception:
            pass
        conn.close()
        log("replied in %.2fs (%d chars)" % (time.time() - started, len(reply)))


# ── server ───────────────────────────────────────────────────────────────────
def serve():
    init_system_prompt()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind((HOST, PORT))
    except OSError as e:
        log("cannot bind %s:%d — %s" % (HOST, PORT, e))
        sys.exit(1)
    srv.listen(64)

    log("listening on %s:%d" % (HOST, PORT))
    log("model=%s  http_timeout=%.1fs" % (MODEL, HTTP_TIMEOUT))
    if not API_KEY:
        log("WARNING: OPENROUTER_API_KEY is not set; every request will return "
            "a config error until you set it and restart.")
    if HTTP_TIMEOUT >= 5.0:
        log("WARNING: AI_GATEWAY_TIMEOUT >= 5s; the shell's fetch aborts at 5s, "
            "so replies may not arrive in time.")

    try:
        while True:
            conn, addr = srv.accept()
            threading.Thread(target=handle_conn, args=(conn, addr),
                             daemon=True).start()
    except KeyboardInterrupt:
        log("shutting down (SIGINT)")
    finally:
        srv.close()


if __name__ == "__main__":
    serve()
