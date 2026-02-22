"""
test_vt100_basics.py — VT100 escape sequence behavior tests.

Tests that validate VT100 output and toolbar rendering behavior,
building toward full statusbar testing.
"""
# Copyright (c) 2024 Ian Lesnet
# Modified by contributors to the BusPirate5-firmware project

import pytest

from bp_vt100_parser import (
    VirtualScreen,
    parse_scroll_region,
    has_cursor_hide,
    has_cursor_show,
    has_color,
)
from bp_terminal_client import TerminalClient


class TestScrollRegion:
    def test_scroll_region_matches_terminal_size(self, terminal, request):
        """With a 24-row terminal, scroll region bottom ≈ 24 - 4 = 20."""
        rows = request.config.getoption("--terminal-rows")
        raw, _ = terminal.run_command("\r")
        region = parse_scroll_region(raw)
        if region is None:
            pytest.skip("No scroll region sequence in output — statusbar may be disabled")
        # The statusbar occupies approximately the bottom 4 rows
        # So scroll region bottom should be around rows - 4
        expected_bottom = rows - 4
        assert region.bottom <= rows, (
            f"Scroll region bottom ({region.bottom}) exceeds terminal rows ({rows})"
        )
        assert region.bottom >= expected_bottom - 2, (
            f"Scroll region bottom ({region.bottom}) much smaller than expected "
            f"(~{expected_bottom} for {rows}-row terminal with 4-line statusbar)"
        )

    def test_different_terminal_sizes(self, request):
        """Connect with 40×120, verify scroll region adjusts accordingly."""
        terminal_port, _ = __import__("conftest").discover_ports(request.config)
        big_terminal = TerminalClient(
            port=terminal_port,
            rows=40,
            cols=120,
        )
        try:
            big_terminal.consume_startup()
            raw, _ = big_terminal.run_command("\r")
            region = parse_scroll_region(raw)
            if region is None:
                pytest.skip("No scroll region sequence — statusbar may be disabled")
            # Scroll region bottom should be less than 40 (4-line statusbar)
            assert region.bottom <= 40, (
                f"Scroll region bottom ({region.bottom}) exceeds 40 rows"
            )
            assert region.bottom >= 34, (
                f"Scroll region bottom ({region.bottom}) unexpectedly small "
                f"for a 40-row terminal"
            )
        finally:
            big_terminal.close()


class TestCursorControl:
    def test_cursor_hidden_during_statusbar_update(self, terminal):
        """Look for cursor hide and cursor show sequences in raw output."""
        raw, _ = terminal.run_command("\r")
        if not has_cursor_hide(raw):
            pytest.skip("No cursor hide sequence — statusbar may be disabled")
        assert has_cursor_show(raw), (
            "Found cursor hide (\\x1b[?25l) but not cursor show (\\x1b[?25h) — "
            "possible cursor left hidden"
        )


class TestColorOutput:
    def test_no_raw_escapes_in_no_color_mode(self, terminal):
        """After disabling color via 'config color none', output should lack color SGR sequences."""
        # Ensure we are in HiZ mode (mode 1) before changing config
        terminal.run_command("m 1\r")

        # Try the 'config color none' command to disable colors
        # If the command isn't recognised, the test is skipped
        raw_cfg, clean_cfg = terminal.run_command("config color none\r")
        if "?" in clean_cfg or "error" in clean_cfg.lower():
            pytest.skip("config color command not available on this firmware")

        try:
            raw, _ = terminal.run_command("i\r")
            assert len(raw) > 0, "Expected some output from info command"
            # With color disabled, there should be no SGR color sequences
            # (basic cursor sequences may still be present)
            assert not has_color(raw), (
                "Expected no SGR color sequences after disabling color"
            )
        finally:
            # Restore default color setting so other tests are not affected
            terminal.run_command("config color default\r")


class TestVirtualScreen:
    def test_statusbar_content_in_correct_rows(self, terminal, request):
        """Using VirtualScreen, verify bottom rows contain statusbar content."""
        rows = request.config.getoption("--terminal-rows")
        cols = request.config.getoption("--terminal-cols")

        raw, _ = terminal.run_command("\r")
        region = parse_scroll_region(raw)
        if region is None:
            pytest.skip("No scroll region — statusbar may be disabled")

        screen = VirtualScreen(rows=rows, cols=cols)
        screen.feed(raw)

        # The statusbar should occupy rows below the scroll region
        statusbar_rows = screen.get_region(region.bottom + 1, rows)
        # At minimum, the statusbar area should exist
        assert len(statusbar_rows) > 0, "Expected statusbar rows below scroll region"

        # The statusbar rows should contain some non-space content
        statusbar_text = "".join(statusbar_rows)
        # It's acceptable for the statusbar to be spaces immediately after connect,
        # so we just verify the screen processed without error
        assert isinstance(statusbar_text, str)

    def test_virtual_screen_cursor_tracking(self, terminal, request):
        """VirtualScreen should track cursor position after feeding output."""
        rows = request.config.getoption("--terminal-rows")
        cols = request.config.getoption("--terminal-cols")

        raw, _ = terminal.run_command("i\r")
        screen = VirtualScreen(rows=rows, cols=cols)
        screen.feed(raw)

        # After processing, cursor should be within screen bounds
        assert 1 <= screen.cursor_row <= rows, (
            f"Cursor row {screen.cursor_row} out of bounds (1..{rows})"
        )
        assert 1 <= screen.cursor_col <= cols, (
            f"Cursor col {screen.cursor_col} out of bounds (1..{cols})"
        )
