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

Wire protocol (must match apps/fetch/cmd_fetch.c exactly):
  * Request  : <prompt text>\\n        (one line, client closes after reading reply)
  * Response : <single clean line>\\n  (NO internal newlines — fetch prints the
               whole buffer verbatim once it sees the first '\\n')

Latency budget: the shell's fetch aborts with "recv: timed out" after 5 s of
silence (FETCH_RECV_TIMEOUT_SECS). We cannot stream keep-alives (fetch would
print them concatenated with the answer), so the full round-trip must land
inside that window. HTTP_TIMEOUT defaults just under 5 s and a timeout returns a
clean one-line error rather than letting the shell hang.

Dependencies: standard library only (socket, threading, urllib). No `requests`.

Usage:
    export OPENROUTER_API_KEY=sk-or-...
    python3 ai_gateway.py
"""

import json
import os
import socket
import sys
import threading
import time
import urllib.error
import urllib.request

# ── configuration ────────────────────────────────────────────────────────────
HOST = os.environ.get("AI_GATEWAY_HOST", "127.0.0.1")
PORT = int(os.environ.get("AI_GATEWAY_PORT", "5001"))

OPENROUTER_URL = "https://openrouter.ai/api/v1/chat/completions"

# Hardcoded high-quality free-tier engine. Override with AI_GATEWAY_MODEL if you
# prefer the other approved free option: "meta-llama/llama-3-8b-instruct:free".
MODEL = os.environ.get("AI_GATEWAY_MODEL", "google/gemini-2.5-flash:free")

API_KEY = os.environ.get("OPENROUTER_API_KEY")

# Keep this just under the shell's 5 s fetch recv timeout so a slow upstream
# yields a graceful one-line error instead of the shell's own timeout message.
HTTP_TIMEOUT = float(os.environ.get("AI_GATEWAY_TIMEOUT", "4.5"))

# Guard rails on the socket side.
MAX_REQUEST_BYTES = 8192      # cap a single prompt line
READ_TIMEOUT_SECS = 30        # don't let a silent client pin a worker thread
MAX_REPLY_CHARS = 2000        # the shell shows this inline; keep it sane

# ── system prompt: sandbox the model to the rahulbox command set ─────────────
ALLOWED_APPS = ("hello", "ls", "stat", "wc", "cat", "echo", "head", "tail",
                "grep", "sort", "uniq", "cut", "tee", "pkg", "fetch")

SYSTEM_PROMPT = (
    'You are the dedicated AI assistant embedded inside "rahulbox", a small '
    "custom Unix-style shell written in C. You are NOT a general-purpose "
    "assistant; you exist only to help the user get things done using "
    "rahulbox's own commands.\n"
    "\n"
    "ABSOLUTE RULES — never break these:\n"
    "\n"
    "1. COMMAND VOCABULARY. You may ONLY use these 15 built-in rahulbox apps, "
    "and nothing else: " + ", ".join(ALLOWED_APPS) + ". Treat every other "
    "program name as nonexistent.\n"
    "\n"
    "2. FORBIDDEN COMMANDS. rahulbox does NOT ship standard coreutils. NEVER "
    "suggest or even mention cp, mv, rm, find, awk, sed, xargs, chmod, or any "
    "utility that is not in the list above. If a task genuinely cannot be done "
    "with the 15 allowed apps, say so plainly in one sentence instead of "
    "inventing a command.\n"
    "\n"
    '3. SYNTAX. The only supported operator is the pipe "|", connecting '
    'commands left to right. NEVER use "&&", "||", ";", subshells, backticks, '
    '"$(...)", or any other shell scripting construct. A valid answer is a '
    "single pipeline, e.g.: cat notes.txt | grep TODO | head.\n"
    "\n"
    "4. STAY ON MISSION. Politely decline anything unrelated to operating the "
    "rahulbox shell. Do not write prose, stories, code in other languages, or "
    "general-knowledge answers.\n"
    "\n"
    "5. OUTPUT FORMAT (critical wire protocol). Reply with ONE single line of "
    "plain text and nothing else. Never use line breaks, carriage returns, "
    "bullet points, markdown, or code fences. Prefer to answer with just the "
    "recommended pipeline, optionally followed by a short dash-separated "
    "clause explaining it, e.g.: ls | grep \".txt\" | sort — lists text files "
    "in order. Keep the whole reply to one concise sentence."
)


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
        "temperature": 0.2,
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
