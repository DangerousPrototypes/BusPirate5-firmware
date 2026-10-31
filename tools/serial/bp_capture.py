#!/usr/bin/env python3
"""
Capture Bus Pirate command output for formatting analysis.
Sends a command, captures the full output, and writes both
raw (with escape codes) and clean versions.
"""

import re
import socket
import sys
import time

_ANSI_RE = re.compile(
    r'\x1b\[[0-9;]*[A-Za-z]'
    r'|\x1b\][^\x07]*\x07'
    r'|\x1b[()][AB012]'
    r'|\x1b[78]'
    r'|\x1b\[\?[0-9;]*[A-Za-z]'
    r'|\x07|\x08'
)

VT100_Q = '\x1b7\x1b[999;999H\x1b[6n\x1b8'

class BPCapture:
    def __init__(self, host='172.26.208.1', port=2217):
        self.host = host
        self.port = port
        self.sock = None
        self._prev = ''

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(0.1)
        self.sock.connect((self.host, self.port))

        # Initial handshake
        self.sock.sendall(b'\r')
        self._read_until_prompt(timeout=6)
        self._drain(1.0)

        # Clean prompt
        self.sock.sendall(b'\r')
        self._read_until_prompt(timeout=3)
        self._drain(0.5)

    def send(self, cmd, timeout=15):
        # Drain stale data
        self._drain(0.3)

        self.sock.sendall((cmd + '\r').encode())
        raw = self._read_until_prompt(timeout=timeout)
        return raw

    def _handle_vt100(self, chunk):
        combined = self._prev + chunk
        if VT100_Q in combined:
            self.sock.sendall(b'[24;80R')
            self._prev = ''
        else:
            self._prev = chunk[-50:] if len(chunk) > 50 else chunk

    def _read_until_prompt(self, timeout=5):
        buf = ''
        deadline = time.time() + timeout
        saw_prompt = False
        prompt_time = 0

        while time.time() < deadline:
            try:
                d = self.sock.recv(4096)
                if d:
                    c = d.decode('utf-8', errors='ignore')
                    self._handle_vt100(c)
                    buf += c
                    if '\x03' in c:
                        saw_prompt = True
                        prompt_time = time.time()
            except socket.timeout:
                if saw_prompt and (time.time() - prompt_time > 0.2):
                    return buf
                time.sleep(0.02)
        return buf

    def _drain(self, duration=0.5):
        deadline = time.time() + duration
        while time.time() < deadline:
            try:
                d = self.sock.recv(4096)
                if d:
                    self._handle_vt100(d.decode('utf-8', errors='ignore'))
            except socket.timeout:
                time.sleep(0.02)

    def close(self):
        if self.sock:
            self.sock.close()


def clean(raw):
    text = raw.replace('\x03', '').replace('\r', '')
    text = _ANSI_RE.sub('', text)
    return text


def main():
    commands = sys.argv[1:] if len(sys.argv) > 1 else ['?']

    bp = BPCapture()
    bp.connect()

    for cmd in commands:
        print(f'=== Command: "{cmd}" ===')
        raw = bp.send(cmd)
        cleaned = clean(raw)
        # Number lines for analysis
        lines = cleaned.split('\n')
        for i, line in enumerate(lines, 1):
            # Show empty lines explicitly
            if line.strip() == '':
                print(f'  {i:3d}: <empty>')
            else:
                print(f'  {i:3d}: {line}')
        print()

    bp.close()


if __name__ == '__main__':
    main()
