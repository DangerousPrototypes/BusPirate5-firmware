#!/usr/bin/env python3
"""
Persistent Bus Pirate shell over TCP serial bridge.

Keeps a single connection open and accepts commands via a FIFO pipe,
so external tools (like an AI agent) can interact without
reconnecting each time.

Usage:
    # Start the shell (background)
    python3 bp_shell.py 172.26.208.1 2217

    # Send commands from another terminal:
    echo "i" > /tmp/bp_cmd

    # Read output:
    cat /tmp/bp_out

    # Interactive mode (default if stdin is a tty):
    python3 bp_shell.py 172.26.208.1 2217 --interactive
"""

import argparse
import os
import re
import socket
import sys
import threading
import time
import select

# VT100 / ANSI stripping
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

CMD_FIFO = "/tmp/bp_cmd"
OUT_FILE = "/tmp/bp_out"
STATUS_FILE = "/tmp/bp_status"


def strip_ansi(text: str) -> str:
    return _ANSI_RE.sub("", text)


def clean_output(raw: str) -> str:
    text = raw.replace("\x03", "").replace("\r", "")
    text = strip_ansi(text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


class BPShell:
    def __init__(self, host: str, port: int, rows: int = 24, cols: int = 80):
        self.host = host
        self.port = port
        self.rows = rows
        self.cols = cols
        self.sock = None
        self._prev_data = ""
        self._lock = threading.Lock()

    def connect(self):
        """Establish TCP connection to the serial bridge."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self.sock.settimeout(0.1)
        print(f"[bp_shell] Connected to {self.host}:{self.port}")

        # Trigger initial prompt and handle VT100 handshake
        self._write(b"\r\n")
        self._read_until_prompt(timeout=5.0)
        print("[bp_shell] Ready.")

    def _write(self, data: bytes):
        with self._lock:
            self.sock.sendall(data)

    def _read_available(self) -> str:
        """Read whatever is available on the socket."""
        buf = ""
        try:
            while True:
                data = self.sock.recv(4096)
                if not data:
                    break
                chunk = data.decode("utf-8", errors="ignore")
                self._handle_vt100(chunk)
                buf += chunk
        except (socket.timeout, BlockingIOError):
            pass
        return buf

    def _read_until_prompt(self, timeout: float = 5.0) -> str:
        """Read until prompt marker or timeout."""
        buf = ""
        deadline = time.time() + timeout
        saw_prompt = False
        prompt_time = 0.0

        while time.time() < deadline:
            try:
                data = self.sock.recv(4096)
                if data:
                    chunk = data.decode("utf-8", errors="ignore")
                    self._handle_vt100(chunk)
                    buf += chunk
                    if PROMPT_MARKER in chunk:
                        saw_prompt = True
                        prompt_time = time.time()
            except (socket.timeout, BlockingIOError):
                if saw_prompt and (time.time() - prompt_time > 0.15):
                    return buf
                time.sleep(0.02)

        return buf

    def _handle_vt100(self, chunk: str):
        """Auto-respond to VT100 cursor-position queries."""
        combined = self._prev_data + chunk
        if VT100_SIZE_QUERY in combined:
            reply = f"[{self.rows};{self.cols}R"
            self.sock.sendall(reply.encode("utf-8"))
            self._prev_data = ""
        else:
            self._prev_data = chunk[-50:] if len(chunk) > 50 else chunk

    def send_command(self, cmd: str, timeout: float = 5.0) -> str:
        """Send a command and return clean output."""
        # Drain any pending data first
        self._read_available()

        self._write((cmd + "\r").encode("utf-8"))
        raw = self._read_until_prompt(timeout=timeout)
        return clean_output(raw)

    def close(self):
        if self.sock:
            self.sock.close()


def setup_fifo():
    """Create the command FIFO if it doesn't exist."""
    if os.path.exists(CMD_FIFO):
        os.remove(CMD_FIFO)
    os.mkfifo(CMD_FIFO)
    # Clear output file
    with open(OUT_FILE, "w") as f:
        f.write("")
    with open(STATUS_FILE, "w") as f:
        f.write("ready\n")


def fifo_mode(shell: BPShell):
    """Listen for commands on the FIFO and write output to file."""
    setup_fifo()
    print(f"[bp_shell] FIFO mode: send commands to {CMD_FIFO}")
    print(f"[bp_shell] Output goes to {OUT_FILE}")
    print(f"[bp_shell] Status in {STATUS_FILE}")
    print(f"[bp_shell] Example: echo 'i' > {CMD_FIFO}")

    try:
        while True:
            # Open FIFO for reading (blocks until a writer appears)
            with open(CMD_FIFO, "r") as fifo:
                for line in fifo:
                    cmd = line.strip()
                    if not cmd:
                        continue
                    if cmd.lower() == "quit":
                        print("[bp_shell] Quit command received.")
                        return

                    # Mark busy
                    with open(STATUS_FILE, "w") as f:
                        f.write("busy\n")

                    print(f"[bp_shell] >>> {cmd}")
                    result = shell.send_command(cmd)
                    print(result)

                    # Write output
                    with open(OUT_FILE, "w") as f:
                        f.write(result + "\n")

                    # Mark ready
                    with open(STATUS_FILE, "w") as f:
                        f.write("ready\n")
    except KeyboardInterrupt:
        pass
    finally:
        if os.path.exists(CMD_FIFO):
            os.remove(CMD_FIFO)


def interactive_mode(shell: BPShell):
    """Interactive REPL."""
    print("[bp_shell] Interactive mode. Type commands, Ctrl+C to quit.")
    try:
        while True:
            try:
                cmd = input("BP> ")
            except EOFError:
                break
            if cmd.strip().lower() == "quit":
                break
            if not cmd.strip():
                continue
            result = shell.send_command(cmd.strip())
            print(result)
    except KeyboardInterrupt:
        print()


def main():
    parser = argparse.ArgumentParser(description="Persistent Bus Pirate shell")
    parser.add_argument("host", help="Serial bridge host (e.g., 172.26.208.1)")
    parser.add_argument("port", type=int, help="Serial bridge TCP port (e.g., 2217)")
    parser.add_argument("--rows", type=int, default=24, help="Terminal rows")
    parser.add_argument("--cols", type=int, default=80, help="Terminal cols")
    parser.add_argument("--interactive", action="store_true",
                        help="Interactive REPL mode (default if stdin is a tty)")
    parser.add_argument("--fifo", action="store_true",
                        help="FIFO pipe mode for automation")
    args = parser.parse_args()

    shell = BPShell(args.host, args.port, rows=args.rows, cols=args.cols)
    try:
        shell.connect()

        if args.fifo:
            fifo_mode(shell)
        elif args.interactive or sys.stdin.isatty():
            interactive_mode(shell)
        else:
            fifo_mode(shell)
    finally:
        shell.close()
        print("[bp_shell] Closed.")


if __name__ == "__main__":
    main()
