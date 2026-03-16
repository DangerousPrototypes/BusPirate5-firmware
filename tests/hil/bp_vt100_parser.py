"""
bp_vt100_parser.py — VT100 escape sequence parser and virtual screen.

Parses raw VT100 output into structured data suitable for test assertions
and provides a simple VirtualScreen for verifying row content.
"""
# Copyright (c) 2024 Ian Lesnet
# Modified by contributors to the BusPirate5-firmware project

import re
from dataclasses import dataclass, field
from typing import Generator


# ---------------------------------------------------------------------------
# Data classes for parsed sequences
# ---------------------------------------------------------------------------

@dataclass
class ScrollRegion:
    top: int
    bottom: int


@dataclass
class CursorPosition:
    row: int
    col: int


@dataclass
class CsiSequence:
    params: str
    command: str


# ---------------------------------------------------------------------------
# Low-level sequence extraction
# ---------------------------------------------------------------------------

# Match any CSI sequence:  ESC [ <params> <cmd>
_CSI_RE = re.compile(r"\x1b\[([0-9;]*?)([A-Za-z])")
# Private mode CSI:  ESC [ ? <params> <cmd>
_CSI_PRIVATE_RE = re.compile(r"\x1b\[\?([0-9;]*)([A-Za-z])")
# Save/restore cursor
_SAVE_RESTORE_RE = re.compile(r"\x1b([78])")


def iter_csi(raw: str) -> Generator[CsiSequence, None, None]:
    """Yield every standard CSI sequence found in *raw*."""
    for m in _CSI_RE.finditer(raw):
        yield CsiSequence(params=m.group(1), command=m.group(2))


def parse_scroll_region(raw: str) -> ScrollRegion | None:
    """Return the last ``\\x1b[top;bottomr`` sequence found, or ``None``."""
    result = None
    for seq in iter_csi(raw):
        if seq.command == "r" and ";" in seq.params:
            parts = seq.params.split(";")
            if len(parts) == 2:
                try:
                    result = ScrollRegion(int(parts[0]), int(parts[1]))
                except ValueError:
                    pass
    return result


def parse_cursor_positions(raw: str) -> list[CursorPosition]:
    """Return all ``\\x1b[row;colH`` cursor-positioning sequences."""
    positions = []
    for seq in iter_csi(raw):
        if seq.command == "H":
            if ";" in seq.params:
                parts = seq.params.split(";")
                try:
                    positions.append(CursorPosition(int(parts[0]), int(parts[1])))
                except ValueError:
                    pass
            else:
                # \x1b[H → home position
                positions.append(CursorPosition(1, 1))
    return positions


def has_cursor_hide(raw: str) -> bool:
    """Return ``True`` if raw output contains ``\\x1b[?25l`` (hide cursor)."""
    return bool(_CSI_PRIVATE_RE.search(raw) and "\x1b[?25l" in raw)


def has_cursor_show(raw: str) -> bool:
    """Return ``True`` if raw output contains ``\\x1b[?25h`` (show cursor)."""
    return "\x1b[?25h" in raw


def has_color(raw: str) -> bool:
    """Return ``True`` if raw output contains any SGR color sequences."""
    for seq in iter_csi(raw):
        if seq.command == "m":
            return True
    return False


def has_save_restore_cursor(raw: str) -> bool:
    """Return ``True`` if raw output contains ``\\x1b7`` or ``\\x1b8``."""
    return bool(_SAVE_RESTORE_RE.search(raw))


# ---------------------------------------------------------------------------
# VirtualScreen
# ---------------------------------------------------------------------------

class VirtualScreen:
    """Minimal VT100 terminal emulator for test assertions.

    Only implements the subset of VT100 needed for Bus Pirate toolbar
    testing: cursor positioning, erase line/chars, scroll region, and
    printable character output.

    Rows and columns are **1-based** to match VT100 convention.
    """

    def __init__(self, rows: int = 24, cols: int = 80) -> None:
        self.rows = rows
        self.cols = cols
        # Internal buffer uses 0-based indexing
        self._buf: list[list[str]] = [[" "] * cols for _ in range(rows)]
        self._cursor_row: int = 1  # 1-based
        self._cursor_col: int = 1  # 1-based
        self._scroll_top: int = 1
        self._scroll_bottom: int = rows

    # ------------------------------------------------------------------
    # Public query helpers
    # ------------------------------------------------------------------

    def get_line(self, row: int) -> str:
        """Return the content of *row* (1-based) as a string."""
        return "".join(self._buf[row - 1])

    def get_region(self, top: int, bottom: int) -> list[str]:
        """Return lines *top* through *bottom* (1-based, inclusive)."""
        return [self.get_line(r) for r in range(top, bottom + 1)]

    @property
    def cursor_row(self) -> int:
        return self._cursor_row

    @property
    def cursor_col(self) -> int:
        return self._cursor_col

    @property
    def scroll_region(self) -> ScrollRegion:
        return ScrollRegion(self._scroll_top, self._scroll_bottom)

    # ------------------------------------------------------------------
    # Feed / process raw terminal output
    # ------------------------------------------------------------------

    def feed(self, raw: str) -> None:
        """Process a raw terminal output string, updating internal state."""
        i = 0
        while i < len(raw):
            ch = raw[i]

            if ch == "\x1b":
                # Escape sequence
                consumed = self._handle_escape(raw, i)
                if consumed > 0:
                    i += consumed
                    continue
                # Unrecognised — skip the ESC
                i += 1

            elif ch == "\r":
                self._cursor_col = 1
                i += 1

            elif ch == "\n":
                if self._cursor_row < self.rows:
                    self._cursor_row += 1
                i += 1

            elif ch == "\x08":  # backspace
                if self._cursor_col > 1:
                    self._cursor_col -= 1
                i += 1

            elif ch == "\x07":  # BEL — ignore
                i += 1

            elif ch == "\x03":  # ETX prompt marker — ignore
                i += 1

            elif ch >= " ":  # printable
                r = self._cursor_row - 1
                c = self._cursor_col - 1
                if 0 <= r < self.rows and 0 <= c < self.cols:
                    self._buf[r][c] = ch
                if self._cursor_col < self.cols:
                    self._cursor_col += 1
                i += 1

            else:
                i += 1

    # ------------------------------------------------------------------
    # Internal escape-sequence handler
    # ------------------------------------------------------------------

    def _handle_escape(self, raw: str, pos: int) -> int:
        """Process escape sequence starting at *pos*.

        Returns the number of characters consumed, or 0 if unrecognised.
        """
        if pos + 1 >= len(raw):
            return 0

        next_ch = raw[pos + 1]

        # Save / restore cursor  \x1b7  \x1b8
        if next_ch in "78":
            # We don't track a cursor stack — just acknowledge
            return 2

        # CSI sequence  \x1b[...
        if next_ch == "[":
            # Find the end of the CSI sequence
            j = pos + 2
            while j < len(raw) and (raw[j].isdigit() or raw[j] in ";?"):
                j += 1
            if j >= len(raw):
                return 0  # incomplete — don't consume

            params_str = raw[pos + 2: j]
            cmd = raw[j]
            total = j - pos + 1

            # Strip leading '?' for private modes
            private = params_str.startswith("?")
            params = params_str.lstrip("?")

            self._apply_csi(params, cmd, private)
            return total

        return 0

    def _apply_csi(self, params: str, cmd: str, private: bool) -> None:
        """Apply a parsed CSI sequence to the virtual screen state."""
        if private:
            # e.g. \x1b[?25l / \x1b[?25h — cursor show/hide, no screen change
            return

        if cmd == "H":
            # Cursor position
            if params:
                parts = params.split(";")
                row = int(parts[0]) if parts[0] else 1
                col = int(parts[1]) if len(parts) > 1 and parts[1] else 1
            else:
                row, col = 1, 1
            self._cursor_row = max(1, min(row, self.rows))
            self._cursor_col = max(1, min(col, self.cols))

        elif cmd == "K":
            # Erase line
            p = int(params) if params else 0
            r = self._cursor_row - 1
            if p == 0:
                # Erase from cursor to end of line
                for c in range(self._cursor_col - 1, self.cols):
                    self._buf[r][c] = " "
            elif p == 1:
                # Erase from beginning of line to cursor
                for c in range(0, self._cursor_col):
                    self._buf[r][c] = " "
            elif p == 2:
                # Erase entire line
                self._buf[r] = [" "] * self.cols

        elif cmd == "X":
            # Erase characters
            n = int(params) if params else 1
            r = self._cursor_row - 1
            for c in range(self._cursor_col - 1, min(self._cursor_col - 1 + n, self.cols)):
                self._buf[r][c] = " "

        elif cmd == "r":
            # Set scroll region
            if ";" in params:
                parts = params.split(";")
                try:
                    self._scroll_top = int(parts[0])
                    self._scroll_bottom = int(parts[1])
                except ValueError:
                    pass

        elif cmd == "A":
            # Cursor up
            n = int(params) if params else 1
            self._cursor_row = max(1, self._cursor_row - n)

        elif cmd == "B":
            # Cursor down
            n = int(params) if params else 1
            self._cursor_row = min(self.rows, self._cursor_row + n)

        elif cmd == "C":
            # Cursor forward
            n = int(params) if params else 1
            self._cursor_col = min(self.cols, self._cursor_col + n)

        elif cmd == "D":
            # Cursor back
            n = int(params) if params else 1
            self._cursor_col = max(1, self._cursor_col - n)

        elif cmd == "J":
            # Erase display
            p = int(params) if params else 0
            r = self._cursor_row - 1
            if p == 0:
                for c in range(self._cursor_col - 1, self.cols):
                    self._buf[r][c] = " "
                for row in range(r + 1, self.rows):
                    self._buf[row] = [" "] * self.cols
            elif p == 2:
                self._buf = [[" "] * self.cols for _ in range(self.rows)]
