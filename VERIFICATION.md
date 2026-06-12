# Manual Verification Guide — rahulbox

How to manually verify the `mkdir` app, the `ls` hardening, and the `ftpd`
FTP daemon (active **and** passive mode). Every command below has been run
against a live build.

> Paths assume the repo is at `~/rahulbox`. Adjust if yours differs.

---

## 0. Build everything

```bash
cd ~/rahulbox
make            # builds all apps + the mysh shell
```

Expected: compiles with no errors/warnings. Key artifacts: `apps/mkdir/mkdir`,
`apps/ftpd/ftpd`, `apps/ls/ls`, `shell/mysh`.

---

## 1. `mkdir` app

**Standalone binary:**

```bash
cd /tmp && rm -rf mk && mkdir mk && cd mk
~/rahulbox/apps/mkdir/mkdir --help                       # help from the argtable
~/rahulbox/apps/mkdir/mkdir -p a/b/c && ls -R a          # deep parents created
~/rahulbox/apps/mkdir/mkdir -p a/b/c ; echo "exit=$?"    # -p idempotent  -> exit=0
~/rahulbox/apps/mkdir/mkdir a/b/c    ; echo "exit=$?"    # exists, no -p   -> exit=1
```

**Errors go to the output stream, not stderr** (the property that lets the FTP
daemon reuse it):

```bash
~/rahulbox/apps/mkdir/mkdir a/b/c 2>/dev/null            # message STILL prints (on stdout)
```

Expected: `mkdir: cannot create directory 'a/b/c': File exists` still appears.

---

## 2. `ls` hardening (JSON escaping + thread-safe errors)

```bash
cd /tmp && rm -rf lst && mkdir lst && cd lst
touch 'evil".json","injected":"pwned'                    # hostile filename
touch normal.txt

~/rahulbox/apps/ls/ls --json .                           # quotes are \"-escaped
~/rahulbox/apps/ls/ls --json . | python3 -m json.tool    # MUST parse cleanly
```

Expected: `json.tool` pretty-prints without error and the hostile name is a
single `name` value (the `"injected"` text did **not** become a JSON key).

**Error path** (thread-safe `strerror_r`):

```bash
~/rahulbox/apps/ls/ls /no/such/dir ; echo "exit=$?"      # -> ... No such file or directory, exit=1
```

---

## 3. `ftpd` FTP daemon

Use **two terminals**. The daemon supports both passive (`PASV`) and active
(`PORT`) data connections, so standard clients work out of the box.

### 3.1 — Terminal A: start the daemon

```bash
mkdir -p /tmp/ftproot && cd /tmp/ftproot
echo "hello from the server" > readme.txt
~/rahulbox/apps/ftpd/ftpd -p 21021 -t 60
```

Expected: `ftpd: listening on port 21021 (timeout 60s; Ctrl-C to stop)`.
It logs each connection. Stop it later with **Ctrl-C**.

> JSON log mode: `ftpd --json -p 21021` emits `{"event":"listening",...}` lines.

### 3.2 — Terminal B: passive mode with standard clients (the common case)

Passive is the default for modern clients, so **no special flags are needed**.

**Plain `curl`:**

```bash
# LIST
curl -s --user me:x "ftp://127.0.0.1:21021/"

# RETR (download)
curl -s --user me:x "ftp://127.0.0.1:21021/readme.txt"

# STOR (upload)
echo "uploaded content" > /tmp/up.txt
curl -s --user me:x -T /tmp/up.txt "ftp://127.0.0.1:21021/up.txt"
cat /tmp/ftproot/up.txt           # verify it landed
```

Expected: LIST prints `readme.txt`; RETR prints `hello from the server`;
STOR lands `uploaded content` in `/tmp/ftproot/up.txt`. (Any username/password
is accepted — there is no real auth.)

**Interactive `ftp` client** (also passive by default):

```bash
ftp -inv 127.0.0.1 21021 <<'EOF'
user me x
get readme.txt /tmp/got.txt
put /tmp/up.txt up2.txt
mkdir madebyftp
ls
bye
EOF
```

Expected: you'll see `227 Entering Passive Mode (...)`, `150`/`226` around each
transfer, `257 "madebyftp" created.`, and the final `ls` listing. Verify with
`ls /tmp/ftproot` (shows `up2.txt`, `madebyftp`) and `cat /tmp/got.txt`.

### 3.3 — Terminal B: control channel via `netcat` (no data connection)

`USER`, `MKD`, and `QUIT` need no data connection, so plain `nc` works:

```bash
printf 'USER me\r\nMKD newdir\r\nQUIT\r\n' | nc -w2 127.0.0.1 21021
ls -ld /tmp/ftproot/newdir
```

Expected replies: `220`, `230 Login successful.`, `257 "newdir" created.`,
`221 Goodbye.`, and `newdir` exists on disk.

Confirm the **allowlist gate** (a command before `USER` is refused):

```bash
printf 'MKD nope\r\nQUIT\r\n' | nc -w2 127.0.0.1 21021   # -> 530 Not logged in
```

### 3.4 — Terminal B: active mode (PORT)

The daemon also speaks classic active mode. With `curl`, force it via `-P -`
(use the local address) and `--disable-eprt` (use `PORT`, not `EPRT`):

```bash
curl -s -P - --disable-eprt --user me:x "ftp://127.0.0.1:21021/"
```

With the interactive `ftp` client, toggle passive off first: type `passive`
at the `ftp>` prompt (it should report "Passive mode: off"), then `ls`/`get`.

### 3.5 — Binary NUL-safety (key RETR/STOR requirement)

Works in either mode; this uses passive:

```bash
head -c 16384 /dev/urandom > /tmp/bin.in        # random bytes, full of NULs
curl -s --user me:x -T /tmp/bin.in "ftp://127.0.0.1:21021/bin.dat"
curl -s --user me:x "ftp://127.0.0.1:21021/bin.dat" -o /tmp/bin.out
cmp /tmp/bin.in /tmp/bin.out && echo "IDENTICAL — binary-safe, no NUL truncation"
```

Expected: `IDENTICAL — binary-safe, no NUL truncation`.

### 3.6 — Concurrency (daemon survives many clients)

```bash
for i in 1 2 3 4 5; do
  curl -s --user u$i:x "ftp://127.0.0.1:21021/" >/dev/null &
done; wait; echo "all concurrent clients done — daemon still listening"
```

Expected: completes with no errors; Terminal A keeps serving (try another `curl`).

---

## 4. Run `ftpd` inside the shell (optional)

```bash
cd /tmp/ftproot
~/rahulbox/shell/mysh
```

At the `mysh>` prompt: `help` lists `mkdir` and `ftpd`; run `ftpd -p 21021`
(blocks, serving — Ctrl-C returns to the shell) or background it with
`ftpd -p 21021 &`. Exit the shell with **Ctrl-D**.

---

## 5. Cleanup

```bash
rm -rf /tmp/ftproot /tmp/mk /tmp/lst /tmp/bin.* /tmp/up.txt /tmp/got.txt
# and Ctrl-C the daemon in Terminal A
```

---

## Notes

- **No special flags needed for passive.** Clients negotiate `EPSV`→`PASV`
  automatically; the daemon answers `PASV` with `227 Entering Passive Mode`.
- **Active mode (`PORT`)** is still fully supported for clients/scripts that
  prefer it (`curl -P -`, or `passive` off in the `ftp` client).
- **Auth is nominal.** Any `USER` succeeds (`230`); there is no `PASS` check.
  This is a teaching-scale daemon serving its own working directory; `CWD` is
  not implemented, so it always lists/serves the directory it was started in.
- **Supported verbs:** `USER`, `QUIT`, `NOOP`, `TYPE`, `SYST`, `PWD`/`XPWD`,
  `PORT`, `PASV`, `MKD`, `LIST`/`NLST`, `RETR`, `STOR`. Anything else → `502`.
