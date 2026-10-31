#!/usr/bin/env python3
"""
Send a single command to the Bus Pirate via TCP serial bridge.

Handles the VT100 handshake properly each time and prints clean output.

Usage:
    python3 bp_cmd.py "i"
    python3 bp_cmd.py "m" --host 172.26.208.1 --port 2217 --timeout 10
    python3 bp_cmd.py ""       # just connect and get the prompt (status check)
"""

import argparse
import re
import socket
import sys
import time

_ANSI_RE = re.compile(
    r"""
      \x1b \[ [0-9;]* [A-Za-z]
    | \x1b \] [^\x07]* \x07
    | \x1b [()][AB012]
    | \x1b [78]
    | \x1b \[ \? [0-9;]* [A-Za-z]
    | \x07
    | \x08
    """,
    re.VERBOSE,
)

VT100_SIZE_QUERY = "\x1b7\x1b[999;999H\x1b[6n\x1b8"
PROMPT_MARKER = "\x03"


def strip_ansi(text: str) -> str:
    return _ANSI_RE.sub("", text)


def clean_output(raw: str) -> str:
    text = raw.replace("\x03", "").replace("\r", "")
    text = strip_ansi(text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def handle_vt100(sock, chunk: str, prev: str, rows: int = 24, cols: int = 80) -> str:
    """Auto-respond to VT100 queries. Returns updated prev buffer."""
    combined = prev + chunk
    if VT100_SIZE_QUERY in combined:
        reply = f"[{rows};{cols}R"
        sock.sendall(reply.encode("utf-8"))
        return ""
    return chunk[-50:] if len(chunk) > 50 else chunk


def read_until_prompt(sock, timeout: float = 5.0, rows: int = 24, cols: int = 80) -> str:
    """Read until prompt marker or timeout."""
    buf = ""
    prev = ""
    deadline = time.time() + timeout
    saw_prompt = False
    prompt_time = 0.0

    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if data:
                chunk = data.decode("utf-8", errors="ignore")
                prev = handle_vt100(sock, chunk, prev, rows, cols)
                buf += chunk
                if PROMPT_MARKER in chunk:
                    saw_prompt = True
                    prompt_time = time.time()
        except socket.timeout:
            if saw_prompt and (time.time() - prompt_time > 0.15):
                return buf
            time.sleep(0.02)

    return buf


def drain(sock, idle_gap: float = 0.3, max_time: float = 3.0, rows: int = 24, cols: int = 80):
    """Read and discard all pending output until quiet."""
    prev = ""
    deadline = time.time() + max_time
    idle_start = time.time()

    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if data:
                chunk = data.decode("utf-8", errors="ignore")
                prev = handle_vt100(sock, chunk, prev, rows, cols)
                idle_start = time.time()
        except socket.timeout:
            if time.time() - idle_start > idle_gap:
                return
            time.sleep(0.02)


def main():
    parser = argparse.ArgumentParser(description="Send command to Bus Pirate via TCP bridge")
    parser.add_argument("command", nargs="?", default="", help="Command to send")
    parser.add_argument("--host", default="172.26.208.1", help="Bridge host")
    parser.add_argument("--port", type=int, default=2217, help="Bridge TCP port")
    parser.add_argument("--timeout", type=float, default=5.0, help="Response timeout")
    parser.add_argument("--rows", type=int, default=24)
    parser.add_argument("--cols", type=int, default=80)
    parser.add_argument("--raw", action="store_true", help="Print raw output (with ANSI)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(0.1)
    try:
        sock.connect((args.host, args.port))
    except ConnectionRefusedError:
        print("ERROR: Cannot connect to bridge. Is serial_bridge.py running on Windows?", file=sys.stderr)
        sys.exit(1)

    try:
        # Initial handshake: send CR, handle VT100 query, drain startup
        sock.sendall(b"\r")
        read_until_prompt(sock, timeout=5.0, rows=args.rows, cols=args.cols)
        drain(sock, idle_gap=0.5, max_time=5.0, rows=args.rows, cols=args.cols)

        # Get to a clean prompt
        sock.sendall(b"\r")
        read_until_prompt(sock, timeout=3.0, rows=args.rows, cols=args.cols)
        drain(sock, idle_gap=0.3, max_time=2.0, rows=args.rows, cols=args.cols)

        # Send the actual command
        if args.command:
            # Drain any remaining data
            try:
                while True:
                    d = sock.recv(4096)
                    if not d:
                        break
            except socket.timeout:
                pass

            sock.sendall((args.command + "\r").encode("utf-8"))
            raw = read_until_prompt(sock, timeout=args.timeout, rows=args.rows, cols=args.cols)

            if args.raw:
                print(raw)
            else:
                print(clean_output(raw))
        else:
            print("Connected OK (no command sent)")

    finally:
        sock.close()


if __name__ == "__main__":
    main()
