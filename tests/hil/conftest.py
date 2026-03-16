"""
conftest.py — pytest fixtures and CLI options for Bus Pirate HIL tests.
"""
# Copyright (c) 2024 Ian Lesnet
# Modified by contributors to the BusPirate5-firmware project

import pytest
import serial.tools.list_ports

from bp_terminal_client import TerminalClient
from bp_bpio_client import BPIOClient
from bp_flash import FlashTool

_BP_VID = 0x1209
_BP_PID = 0x7331


# ---------------------------------------------------------------------------
# pytest CLI options
# ---------------------------------------------------------------------------

def pytest_addoption(parser):
    parser.addoption(
        "--uf2",
        default=None,
        metavar="PATH",
        help="Path to .uf2 firmware file to flash before testing",
    )
    parser.addoption(
        "--terminal-port",
        default=None,
        metavar="PORT",
        help="Override CDC 0 terminal port (default: auto-discover)",
    )
    parser.addoption(
        "--binary-port",
        default=None,
        metavar="PORT",
        help="Override CDC 1 binary port (default: auto-discover)",
    )
    parser.addoption(
        "--terminal-rows",
        type=int,
        default=24,
        metavar="N",
        help="Terminal rows to report to device (default: 24)",
    )
    parser.addoption(
        "--terminal-cols",
        type=int,
        default=80,
        metavar="N",
        help="Terminal columns to report to device (default: 80)",
    )


# ---------------------------------------------------------------------------
# Port discovery
# ---------------------------------------------------------------------------

def discover_ports(config) -> tuple[str, str]:
    """Find CDC 0 and CDC 1 by VID/PID.

    Returns ``(terminal_port, binary_port)``.
    Skips the test if no device is found.
    """
    terminal_port = config.getoption("--terminal-port")
    binary_port = config.getoption("--binary-port")

    if terminal_port and binary_port:
        return terminal_port, binary_port

    ports = [
        p for p in serial.tools.list_ports.comports()
        if p.vid == _BP_VID and p.pid == _BP_PID
    ]

    if len(ports) < 2:
        pytest.skip(
            f"Bus Pirate device ({_BP_VID:04x}:{_BP_PID:04x}) not found "
            f"(found {len(ports)} port(s), need 2)"
        )

    # Sort by device path — lower path = CDC 0 (terminal), higher = CDC 1 (binary)
    sorted_ports = sorted(ports, key=lambda p: p.device)
    resolved_terminal = terminal_port or sorted_ports[0].device
    resolved_binary = binary_port or sorted_ports[1].device
    return resolved_terminal, resolved_binary


# ---------------------------------------------------------------------------
# Optional firmware flash (session-scoped)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session", autouse=True)
def flash_firmware(request):
    """Flash firmware if ``--uf2`` was passed on the pytest command line."""
    uf2_path = request.config.getoption("--uf2")
    if not uf2_path:
        yield
        return

    terminal_port, binary_port = discover_ports(request.config)
    bpio = BPIOClient(port=binary_port, debug=False)
    flasher = FlashTool(debug=True)

    try:
        bpio.enter_bootloader()
    except Exception as exc:
        pytest.fail(f"Failed to enter bootloader: {exc}")

    flasher.wait_for_bootloader(timeout=10)
    flasher.flash(uf2_path, force_reboot=True)
    new_terminal, new_binary = flasher.wait_for_device(timeout=15)

    # Store discovered ports for other fixtures to use
    request.config._bp_terminal_port = new_terminal
    request.config._bp_binary_port = new_binary

    yield


# ---------------------------------------------------------------------------
# Terminal client fixture (session-scoped)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def terminal(request, flash_firmware):
    """TerminalClient connected to CDC 0.  Yields the client."""
    terminal_port, _ = discover_ports(request.config)
    rows = request.config.getoption("--terminal-rows")
    cols = request.config.getoption("--terminal-cols")

    client = TerminalClient(port=terminal_port, rows=rows, cols=cols)
    client.consume_startup()
    yield client
    client.close()


# ---------------------------------------------------------------------------
# BPIO client fixture (session-scoped)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def bpio(request, flash_firmware):
    """BPIOClient connected to CDC 1.  Yields the client."""
    _, binary_port = discover_ports(request.config)

    client = BPIOClient(port=binary_port)
    yield client
    client.close()


# ---------------------------------------------------------------------------
# Soft reset after each test (function-scoped, autouse)
# ---------------------------------------------------------------------------

@pytest.fixture(autouse=True)
def soft_reset(terminal):
    """After each test, return to HiZ mode via ``m 1``."""
    yield
    # Return to HiZ (mode 1) after every test
    terminal.run_command("m 1\r")
