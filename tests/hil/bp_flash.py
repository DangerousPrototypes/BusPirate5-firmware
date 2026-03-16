"""
bp_flash.py — Firmware flash tool wrapping picotool.

Provides helpers for flashing .uf2 firmware files to a Bus Pirate device
and waiting for USB re-enumeration after reflash.
"""
# Copyright (c) 2024 Ian Lesnet
# Modified by contributors to the BusPirate5-firmware project

import subprocess
import time

import serial.tools.list_ports

_BP_VID = 0x1209
_BP_PID = 0x7331


class FlashTool:
    """Wrapper around ``picotool`` for flashing Bus Pirate firmware.

    Parameters
    ----------
    debug:
        Print subprocess output when ``True``.
    """

    def __init__(self, debug: bool = False) -> None:
        self.debug = debug

    def flash(self, uf2_path: str, force_reboot: bool = True) -> None:
        """Load a .uf2 file onto the connected RP2 device via picotool.

        Parameters
        ----------
        uf2_path:
            Absolute path to the .uf2 firmware file.
        force_reboot:
            Pass ``-f`` to picotool to force reboot after loading.
        """
        cmd = ["picotool", "load", str(uf2_path)]
        if force_reboot:
            cmd.append("-f")
        self._run(cmd)

    def verify(self, uf2_path: str) -> None:
        """Verify that the device firmware matches a .uf2 file.

        Parameters
        ----------
        uf2_path:
            Absolute path to the .uf2 firmware file to compare against.
        """
        self._run(["picotool", "verify", str(uf2_path)])

    def wait_for_bootloader(self, timeout: float = 10.0) -> None:
        """Poll until the RP2 mass-storage bootloader device appears.

        The bootloader shows up as a USB mass storage device labelled
        "RPI-RP2" or "RP2350".

        Raises ``TimeoutError`` if the device is not found within *timeout* seconds.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            ports = serial.tools.list_ports.comports()
            for p in ports:
                desc = (p.description or "").upper()
                if "RPI-RP2" in desc or "RP2350" in desc:
                    return
            # Also check for the mass-storage device via picotool
            try:
                result = subprocess.run(
                    ["picotool", "info"],
                    capture_output=True,
                    timeout=2,
                )
                if result.returncode == 0:
                    return
            except (subprocess.TimeoutExpired, FileNotFoundError):
                pass
            time.sleep(0.5)
        raise TimeoutError(f"RP2 bootloader device not found within {timeout}s")

    def wait_for_device(
        self,
        vid: int = _BP_VID,
        pid: int = _BP_PID,
        timeout: float = 15.0,
    ) -> tuple[str, str]:
        """Poll until two CDC serial ports with the given VID/PID appear.

        Returns ``(cdc0_path, cdc1_path)`` sorted by device path (lower = CDC 0).

        Raises ``TimeoutError`` if two ports are not found within *timeout* seconds.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            ports = [
                p for p in serial.tools.list_ports.comports()
                if p.vid == vid and p.pid == pid
            ]
            if len(ports) >= 2:
                sorted_ports = sorted(ports, key=lambda p: p.device)
                return sorted_ports[0].device, sorted_ports[1].device
            time.sleep(0.5)
        raise TimeoutError(
            f"Bus Pirate CDC ports ({vid:04x}:{pid:04x}) not found within {timeout}s"
        )

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _run(self, cmd: list[str], timeout: float = 30.0) -> subprocess.CompletedProcess:
        """Run a subprocess command with error checking."""
        if self.debug:
            print(f"[picotool] {' '.join(cmd)}")
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"Command failed: {' '.join(cmd)}\n"
                f"stderr: {result.stderr}\n"
                f"stdout: {result.stdout}"
            )
        if self.debug:
            print(result.stdout)
        return result
