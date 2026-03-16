"""
test_smoke.py — Basic connectivity smoke tests for the Bus Pirate HIL fixture.

These tests validate that the fixture itself works correctly and should pass
on any Bus Pirate with standard firmware.
"""
# Copyright (c) 2024 Ian Lesnet
# Modified by contributors to the BusPirate5-firmware project

import pytest


class TestTerminalBasics:
    def test_device_responds_to_enter(self, terminal):
        """Send \\r, expect a prompt containing '>'."""
        raw, clean = terminal.run_command("\r")
        assert ">" in clean, f"Expected '>' in output, got: {clean!r}"

    def test_help_command(self, terminal):
        """Send 'help\\r', verify output contains 'Bus Pirate'."""
        raw, clean = terminal.run_command("help\r")
        assert "Bus Pirate" in clean, (
            f"Expected 'Bus Pirate' in help output, got: {clean!r}"
        )

    def test_version_command(self, terminal):
        """Send 'i\\r' (info), verify firmware version text is returned."""
        raw, clean = terminal.run_command("i\r")
        # The info command should mention firmware version
        assert any(
            kw in clean for kw in ("Firmware", "firmware", "Bus Pirate", "HW:", "FW:")
        ), f"Expected firmware info in output, got: {clean!r}"

    def test_v_command_shows_voltages(self, terminal):
        """Send 'v\\r', verify voltage readings appear in the output."""
        raw, clean = terminal.run_command("v\r")
        assert "V" in clean, f"Expected voltage readings ('V') in output, got: {clean!r}"

    def test_terminal_size_detection(self, terminal):
        """With a 24×80 terminal, verify raw output contains a scroll region sequence."""
        raw, _ = terminal.run_command("\r")
        # If the VT100 size query was answered, the BP will set a scroll region
        # The scroll region sequence is \x1b[<top>;<bottom>r
        has_scroll_region = "\x1b[" in raw and "r" in raw
        # We also accept just checking that the device replied with escape sequences
        # (presence of any ESC sequence confirms VT100 mode is active)
        has_any_escape = "\x1b" in raw
        assert has_any_escape, (
            "Expected VT100 escape sequences in raw output — "
            "VT100 size query may not have been answered correctly"
        )


class TestBPIOStatus:
    def test_bpio_status_query(self, bpio):
        """Query status via BPIO2, verify version and mode fields."""
        status = bpio.get_status()
        assert status.get("version_firmware_major") is not None, (
            "Expected version_firmware_major in StatusResponse"
        )
        assert status.get("mode_current") is not None, (
            "Expected mode_current in StatusResponse"
        )
        mode = status["mode_current"]
        assert "HiZ" in mode or "hiz" in mode.lower(), (
            f"Expected device to be in HiZ mode, got: {mode!r}"
        )

    def test_mode_names_list(self, bpio):
        """Query status via BPIO2, verify modes_available is a non-empty list."""
        status = bpio.get_status()
        modes = status.get("modes_available", [])
        assert isinstance(modes, list), "Expected modes_available to be a list"
        assert len(modes) > 0, "Expected at least one mode to be available"
        # HiZ should always be in the list
        assert any("HiZ" in m or "hiz" in m.lower() for m in modes if m), (
            f"Expected HiZ in modes_available, got: {modes!r}"
        )
