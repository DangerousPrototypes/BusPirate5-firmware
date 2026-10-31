#!/usr/bin/env python3

import argparse
import random
import statistics
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")


EPILOG = """
Conformance and robustness test for the Bus Pirate "Legacy Binary Mode for
Flashrom and AVRdude".

WHAT IT PROVES
  Every command in this protocol has a reply whose length is fully determined
  by the request, so a single USB byte lost, duplicated or reordered shows up
  immediately as a short read, a long read or a misplaced ack. The suites drive
  the sizes where a ring buffer or packet boundary bug hides: the 64 byte USB FS
  packet, the 64 byte CDC FIFO, the 128 byte bin_rx_fifo, the 1024 byte
  bin_tx_fifo and the 16384 byte command buffer. On top of that it replays the
  real flashrom and avrdude sequences, sweeps every opcode nibble, and checks
  that the firmware recovers from garbage, abandoned commands and a host that
  disappears.

BEFORE RUNNING
  1. On the Bus Pirate terminal, enter the legacy binary mode and answer the
     prompts. Leave that terminal open.
  2. Pass the SECOND CDC port of the Bus Pirate with --port (use --list).

SAFETY
  The normal suites only use opcodes 0x05 and 0x10, which never assert CS#, and
  leave the power supply off, so a flash chip on the bus is never selected.
  --use-cs and --fuzz are different: --use-cs drives real selected transactions,
  and the fuzzer sends random opcodes which can toggle the power supply, the
  pull-ups and the pin directions. Run those with nothing attached.

LOOPBACK
  Jumper IO7 (MOSI) to IO4 (MISO) and pass --loopback. Opcode 0x10 then becomes
  a byte exact echo, which verifies payload CONTENT and not just framing.

TERMINAL TESTS
  Pass --terminal COMx with the first CDC port to also exercise the 'q' exit
  prompt. Add --exit-test to confirm the exit for real; that resets the Bus
  Pirate and must be the last thing you run.

EXAMPLES
  python legacy4third_usbtest.py --list
  python legacy4third_usbtest.py --port COM19
  python legacy4third_usbtest.py --port COM19 --loopback --soak 300
  python legacy4third_usbtest.py --port COM19 --terminal COM18 --fuzz-rounds 20
"""


BBIO_ID = b"BBIO1"
SPI_ID = b"SPI1"
ACK = 0x01
NAK = 0x00

TMPBUFF_SIZE = 0x4000
PAYLOAD_TIMEOUT = 3.0

BOUNDARY_SIZES = [
    1, 2, 3, 15, 16, 17,
    31, 32, 33,
    63, 64, 65,
    95, 96, 97,
    127, 128, 129,
    191, 192, 193,
    255, 256, 257,
    319, 320, 321,
    511, 512, 513,
    1023, 1024, 1025,
    1087, 1088,
    2047, 2048, 2049,
    4095, 4096,
]

WRITE_SIZES = [0, 1, 2, 4, 5, 16, 63, 64, 65, 127, 128, 129, 255, 256, 257]


class Failure(Exception):
    pass


def hexdump(data, limit=48):
    body = " ".join("%02x" % b for b in data[:limit])
    if len(data) > limit:
        body += " ... (%d bytes total)" % len(data)
    return body


class Link:
    def __init__(self, port, inactivity_timeout, hard_timeout):
        self.ser = serial.Serial(port, 115200, timeout=0.05, write_timeout=10.0)
        self.ser.dtr = True
        self.inactivity_timeout = inactivity_timeout
        self.hard_timeout = hard_timeout
        self.tx_bytes = 0
        self.rx_bytes = 0

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass

    def write(self, data):
        data = bytes(data)
        self.ser.write(data)
        self.ser.flush()
        self.tx_bytes += len(data)

    def read_exact(self, count, what="response", inactivity=None):
        if count == 0:
            return b""
        limit = inactivity if inactivity is not None else self.inactivity_timeout
        buf = bytearray()
        last_progress = time.monotonic()
        hard_stop = last_progress + self.hard_timeout
        while len(buf) < count:
            now = time.monotonic()
            if now - last_progress > limit:
                raise Failure(
                    "%s: stalled after %d of %d bytes (%.2fs without data) [%s]"
                    % (what, len(buf), count, limit, hexdump(buf))
                )
            if now > hard_stop:
                raise Failure(
                    "%s: hard timeout after %d of %d bytes [%s]"
                    % (what, len(buf), count, hexdump(buf))
                )
            chunk = self.ser.read(count - len(buf))
            if chunk:
                buf += chunk
                last_progress = time.monotonic()
        self.rx_bytes += len(buf)
        return bytes(buf)

    def expect_silence(self, seconds=0.05, what="stream"):
        deadline = time.monotonic() + seconds
        extra = bytearray()
        while time.monotonic() < deadline:
            chunk = self.ser.read(64)
            if chunk:
                extra += chunk
        if extra:
            self.rx_bytes += len(extra)
            raise Failure(
                "%s: %d unexpected extra bytes [%s]" % (what, len(extra), hexdump(extra))
            )

    def drain(self, quiet=0.25, cap=6.0):
        deadline = time.monotonic() + cap
        last = time.monotonic()
        dropped = 0
        while time.monotonic() < deadline:
            chunk = self.ser.read(512)
            if chunk:
                dropped += len(chunk)
                last = time.monotonic()
            elif time.monotonic() - last > quiet:
                break
        self.rx_bytes += dropped
        return dropped


class Runner:
    def __init__(self, link, args):
        self.link = link
        self.args = args
        self.passed = 0
        self.failed = 0
        self.skipped = 0
        self.failures = []
        self.commands = 0
        self.latencies = []

    def run(self, name, fn):
        sys.stdout.write("  %-46s " % name)
        sys.stdout.flush()
        started = time.monotonic()
        try:
            detail = fn()
        except Failure as exc:
            self.failed += 1
            self.failures.append((name, str(exc)))
            print("FAIL")
            print("      %s" % exc)
            return False
        except serial.SerialException as exc:
            self.failed += 1
            self.failures.append((name, "serial error: %s" % exc))
            print("FAIL")
            print("      serial error: %s" % exc)
            return False
        elapsed = time.monotonic() - started
        self.passed += 1
        print("ok    %6.2fs  %s" % (elapsed, detail or ""))
        return True

    def skip(self, name, why):
        self.skipped += 1
        print("  %-46s skip            %s" % (name, why))


def bbio_reset(link, count=20, spacing=0.0, inactivity=None):
    for _ in range(count):
        link.write(b"\x00")
        if spacing:
            time.sleep(spacing)
    got = link.read_exact(len(BBIO_ID), "BBIO identifier", inactivity)
    if got != BBIO_ID:
        raise Failure("expected %r, got %r" % (BBIO_ID, got))


def spi_enter(link):
    link.write(b"\x01")
    got = link.read_exact(len(SPI_ID), "SPI identifier")
    if got != SPI_ID:
        raise Failure("expected %r, got %r" % (SPI_ID, got))


def expect_ack(link, opcode, what):
    link.write(bytes([opcode]))
    got = link.read_exact(1, what)
    if got[0] != ACK:
        raise Failure("%s: expected ack 0x01, got 0x%02x" % (what, got[0]))


def configure(link, args):
    peripherals = 0x40 | 0x01
    if args.psu:
        peripherals |= 0x08
    expect_ack(link, peripherals, "peripheral config")
    expect_ack(link, 0x60 | args.spispeed, "spi speed")
    expect_ack(link, 0x80 | 0x0A, "spi config")
    expect_ack(link, 0x03, "cs deassert")


def full_init(link, args):
    link.drain()
    bbio_reset(link)
    spi_enter(link)
    configure(link, args)


def cmd_write_read(link, payload, readcnt, use_cs=False, runner=None):
    opcode = 0x04 if use_cs else 0x05
    writecnt = len(payload)
    header = bytes([
        opcode,
        (writecnt >> 8) & 0xFF, writecnt & 0xFF,
        (readcnt >> 8) & 0xFF, readcnt & 0xFF,
    ])
    started = time.monotonic()
    link.write(header + bytes(payload))
    resp = link.read_exact(1 + readcnt, "0x%02x w=%d r=%d" % (opcode, writecnt, readcnt))
    if runner is not None:
        runner.latencies.append((time.monotonic() - started) * 1000.0)
    if resp[0] != ACK:
        raise Failure(
            "0x%02x w=%d r=%d: expected ack 0x01, got 0x%02x [%s]"
            % (opcode, writecnt, readcnt, resp[0], hexdump(resp))
        )
    return resp[1:]


def cmd_bulk(link, payload):
    n = len(payload)
    if not 1 <= n <= 16:
        raise Failure("bulk transfer takes 1..16 bytes, got %d" % n)
    link.write(bytes([0x10 | (n - 1)]))
    ack = link.read_exact(1, "bulk ack")
    if ack[0] != ACK:
        raise Failure("bulk ack: expected 0x01, got 0x%02x" % ack[0])
    link.write(bytes(payload))
    return link.read_exact(n, "bulk echo")


def suite_handshake(runner, args):
    link = runner.link
    print("\n[handshake] flashrom and avrdude entry sequences")

    def avrdude_style():
        link.drain()
        for _ in range(20):
            link.write(b"\x00")
        got = link.read_exact(5, "avrdude BBIO read")
        if got != BBIO_ID:
            raise Failure(
                "avrdude does a fixed 5 byte read; expected %r got %r" % (BBIO_ID, got)
            )
        link.expect_silence(0.15, "after 20 zeros")
        link.write(b"\x01")
        got = link.read_exact(4, "avrdude SPI read")
        if got != SPI_ID:
            raise Failure(
                "avrdude does a fixed 4 byte read; expected %r got %r" % (SPI_ID, got)
            )
        link.expect_silence(0.15, "after SPI enter")
        return "one BBIO1 per burst, one SPI1"

    def flashrom_full_session():
        link.drain()
        for _ in range(20):
            link.write(b"\x00")
            time.sleep(0.010)
        if link.read_exact(4, "wait_for_string BBIO") != b"BBIO":
            raise Failure("first burst did not start with BBIO")
        link.write(b"\x0f")
        text = bytearray(link.read_exact(1, "leftover version digit"))
        deadline = time.monotonic() + 1.5
        while time.monotonic() < deadline and b"HiZ>" not in bytes(text):
            chunk = link.ser.read(64)
            if chunk:
                text += chunk
        text = bytes(text)
        link.rx_bytes += len(text) - 1
        if not text.startswith(b"1"):
            raise Failure("expected the leftover '1' of BBIO1, got %r" % text[:8])
        for needle in (b"irate ", b"irmware ", b"HiZ>"):
            if needle not in text:
                raise Failure("banner missing %r, got %r" % (needle, text))
        if not text.endswith(b"HiZ>"):
            raise Failure("banner must end at HiZ> with nothing after, got %r" % text[-16:])
        for _ in range(20):
            link.write(b"\x00")
        if link.read_exact(4, "second burst BBIO") != b"BBIO":
            raise Failure("second burst did not start with BBIO")
        version = link.read_exact(1, "bitbang version byte")
        if version != b"1":
            raise Failure("raw bitbang version must be '1', got %r" % version)
        link.write(b"\x01")
        if link.read_exact(3, "wait_for_string SPI") != b"SPI":
            raise Failure("0x01 did not answer SPI")
        version = link.read_exact(1, "spi version byte")
        if version != b"1":
            raise Failure("raw SPI version must be '1', got %r" % version)
        configure(link, args)
        link.expect_silence(0.10, "after flashrom init")
        return "init replayed byte for byte, host buffer left empty"

    def flashrom_shutdown():
        link.write(b"\x00")
        if link.read_exact(4, "shutdown BBIO") != b"BBIO":
            raise Failure("shutdown burst did not answer BBIO")
        if link.read_exact(1, "shutdown version") != b"1":
            raise Failure("shutdown version byte wrong")
        link.write(b"\x0f")
        link.drain()
        full_init(link, args)
        return "shutdown sequence answers and the mode survives"

    def reentry():
        for _ in range(args.reentry):
            full_init(link, args)
        return "%d full BBIO/SPI/config cycles" % args.reentry

    runner.run("avrdude fixed-length handshake", avrdude_style)
    runner.run("flashrom init replay, exact byte accounting", flashrom_full_session)
    runner.run("flashrom shutdown sequence", flashrom_shutdown)
    runner.run("repeated mode re-entry", reentry)


def suite_framing(runner, args):
    link = runner.link
    print("\n[framing] every reply must be exactly the announced length")
    full_init(link, args)

    def single_byte_acks():
        for opcode, name in ((0x02, "cs low"), (0x03, "cs high"),
                             (0x80 | 0x0A, "spi config"), (0x60 | args.spispeed, "speed")):
            expect_ack(link, opcode, name)
            link.expect_silence(0.03, "after %s" % name)
        expect_ack(link, 0x03, "cs high")
        return "0x02/0x03/0x60/0x80 return exactly one byte"

    def zero_length():
        link.write(bytes([0x05, 0, 0, 0, 0]))
        got = link.read_exact(1, "zero length command")
        if got[0] != ACK:
            raise Failure("expected 0x01, got 0x%02x" % got[0])
        link.expect_silence(0.05, "after zero length command")
        return "w=0 r=0 returns exactly one ack"

    def unsupported():
        probes = [0x07, 0x08, 0x0C, 0x0E, 0x20, 0x2F, 0x30, 0x3F,
                  0x50, 0x5F, 0x70, 0x7F, 0x90, 0xA5, 0xC3, 0xFF]
        for opcode in probes:
            link.write(bytes([opcode]))
            got = link.read_exact(1, "unsupported 0x%02x" % opcode)
            if got[0] != NAK:
                raise Failure(
                    "unsupported opcode 0x%02x: expected 0x00, got 0x%02x" % (opcode, got[0])
                )
            link.expect_silence(0.02, "after unsupported 0x%02x" % opcode)
        cmd_write_read(link, b"", 4)
        return "%d unsupported opcodes reply 0x00 and keep sync" % len(probes)

    def peripheral_nibbles():
        for nibble in range(16):
            if not args.psu and (nibble & 0x08):
                continue
            expect_ack(link, 0x40 | nibble, "peripherals 0x%02x" % (0x40 | nibble))
            link.expect_silence(0.02, "after peripherals 0x%02x" % (0x40 | nibble))
        configure(link, args)
        tested = 16 if args.psu else 8
        return "%d peripheral config combinations acked" % tested

    def spi_config_nibbles():
        for nibble in range(16):
            expect_ack(link, 0x80 | nibble, "spi config 0x%02x" % (0x80 | nibble))
            link.expect_silence(0.02, "after spi config 0x%02x" % (0x80 | nibble))
            expect_ack(link, 0x03, "cs high")
            cmd_write_read(link, b"\x9f", 3, args.use_cs)
            runner.commands += 1
        configure(link, args)
        return "16 cpol/cpha/hiz combinations, bus alive after each"

    def speed_table():
        for index in range(16):
            link.write(bytes([0x60 | index]))
            got = link.read_exact(1, "speed index %d" % index)
            want = ACK if index <= 7 else NAK
            if got[0] != want:
                raise Failure(
                    "speed index %d: expected 0x%02x, got 0x%02x" % (index, want, got[0])
                )
            link.expect_silence(0.02, "after speed index %d" % index)
            if want == ACK:
                expect_ack(link, 0x80 | 0x0A, "spi config")
                expect_ack(link, 0x03, "cs high")
                cmd_write_read(link, b"\x9f", 8, args.use_cs)
                runner.commands += 1
        configure(link, args)
        return "indices 0-7 usable end to end, 8-15 rejected"

    def oversized_rejected():
        writecnt = 0x0100
        readcnt = 0x4100
        header = bytes([0x05,
                        (writecnt >> 8) & 0xFF, writecnt & 0xFF,
                        (readcnt >> 8) & 0xFF, readcnt & 0xFF])
        link.write(header + b"\xa5" * writecnt)
        resp = link.read_exact(1 + readcnt, "oversized command")
        if resp[0] != NAK:
            raise Failure("expected 0x00 for w+r>%d, got 0x%02x" % (TMPBUFF_SIZE, resp[0]))
        if any(b != 0xFF for b in resp[1:]):
            raise Failure("padding must be 0xFF, got [%s]" % hexdump(resp[1:]))
        link.expect_silence(0.10, "after oversized command")
        cmd_write_read(link, b"\x9f", 3)
        runner.commands += 2
        return "w+r=%d rejected, payload drained, %d pad bytes, sync kept" % (
            writecnt + readcnt, readcnt
        )

    runner.run("single byte acks", single_byte_acks)
    runner.run("zero length command", zero_length)
    runner.run("unsupported opcodes", unsupported)
    runner.run("peripheral config nibble sweep", peripheral_nibbles)
    runner.run("spi config nibble sweep", spi_config_nibbles)
    runner.run("speed index sweep with traffic", speed_table)
    runner.run("oversized length rejected", oversized_rejected)


def suite_layers(runner, args):
    link = runner.link
    print("\n[layers] bitbang layer must not decode SPI submode opcodes")

    def spi_opcodes_before_enter():
        link.drain()
        bbio_reset(link)
        for opcode in (0x04, 0x05, 0x10, 0x12, 0x13, 0x1F):
            link.write(bytes([opcode]))
            got = link.read_exact(1, "bitbang layer 0x%02x" % opcode)
            if got[0] != NAK:
                raise Failure(
                    "0x%02x before 0x01 must answer 0x00, got 0x%02x" % (opcode, got[0])
                )
            link.expect_silence(0.05, "after bitbang 0x%02x" % opcode)
        spi_enter(link)
        configure(link, args)
        cmd_write_read(link, b"\x9f", 3, args.use_cs)
        runner.commands += 1
        return "0x04/0x05/0x1x answer 0x00 and consume no payload"

    def spi_opcodes_after_leave():
        link.write(b"\x00")
        link.read_exact(len(BBIO_ID), "BBIO after leaving SPI")
        for opcode in (0x05, 0x10):
            link.write(bytes([opcode]))
            got = link.read_exact(1, "left SPI 0x%02x" % opcode)
            if got[0] != NAK:
                raise Failure(
                    "0x%02x after leaving SPI must answer 0x00, got 0x%02x" % (opcode, got[0])
                )
        link.expect_silence(0.05, "after leaving SPI submode")
        full_init(link, args)
        return "leaving the SPI submode re-arms the gate"

    runner.run("SPI opcodes before 0x01", spi_opcodes_before_enter)
    runner.run("SPI opcodes after 0x00", spi_opcodes_after_leave)


def suite_avr(runner, args):
    link = runner.link
    print("\n[avr] extended AVR command set used by avrdude")
    full_init(link, args)

    def avr_noop():
        link.write(b"\x06")
        if link.read_exact(1, "avr enter")[0] != ACK:
            raise Failure("0x06 did not ack")
        link.write(b"\x00")
        if link.read_exact(1, "avr noop")[0] != ACK:
            raise Failure("avr noop did not ack")
        link.expect_silence(0.05, "after avr noop")
        return "0x06 0x00 acks"

    def avr_version():
        link.write(b"\x06")
        link.read_exact(1, "avr enter")
        link.write(b"\x01")
        got = link.read_exact(3, "avr version")
        if got != b"\x01\x00\x01":
            raise Failure("expected 01 00 01, got [%s]" % hexdump(got))
        link.expect_silence(0.05, "after avr version")
        return "protocol version 0x0001"

    def avr_bulk_read():
        length = 16
        link.write(b"\x06")
        link.read_exact(1, "avr enter")
        link.write(b"\x02")
        link.write(bytes([0, 0, 0, 0, 0, 0, 0, length]))
        got = link.read_exact(1 + length, "avr bulk read")
        if got[0] != ACK:
            raise Failure("avr bulk read ack was 0x%02x" % got[0])
        link.expect_silence(0.05, "after avr bulk read")
        return "%d bytes streamed after the ack" % length

    def avr_bulk_bounds():
        cases = [
            (0x00000000, 0x00030000, "length past the 64K word range"),
            (0x00010000, 0x00000010, "address past the 64K word range"),
            (0x0000FFF0, 0x00000100, "address plus length past the range"),
        ]
        for addr, length, why in cases:
            link.write(b"\x06")
            link.read_exact(1, "avr enter")
            link.write(b"\x02")
            link.write(bytes([
                (addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF,
                (length >> 24) & 0xFF, (length >> 16) & 0xFF, (length >> 8) & 0xFF, length & 0xFF,
            ]))
            got = link.read_exact(1, "avr bounds %s" % why)
            if got[0] != NAK:
                stray = link.drain()
                raise Failure(
                    "%s must be rejected, got 0x%02x (drained %d streamed bytes)"
                    % (why, got[0], stray)
                )
            link.expect_silence(0.05, "after avr bounds %s" % why)
        cmd_write_read(link, b"\x9f", 3, args.use_cs)
        runner.commands += 1
        return "%d out of range requests rejected" % len(cases)

    def avr_bad_subcommand():
        for sub in (0x03, 0x7F, 0xFF):
            link.write(b"\x06")
            link.read_exact(1, "avr enter")
            link.write(bytes([sub]))
            got = link.read_exact(1, "avr subcommand 0x%02x" % sub)
            if got[0] != NAK:
                raise Failure("avr subcommand 0x%02x must be rejected" % sub)
            link.expect_silence(0.03, "after avr subcommand 0x%02x" % sub)
        return "unknown subcommands reply 0x00"

    runner.run("0x06 0x00 noop", avr_noop)
    runner.run("0x06 0x01 version", avr_version)
    runner.run("0x06 0x02 bulk read", avr_bulk_read)
    runner.run("0x06 0x02 bounds validation", avr_bulk_bounds)
    runner.run("0x06 unknown subcommand", avr_bad_subcommand)


def suite_sizes(runner, args):
    link = runner.link
    print("\n[sizes] boundary sweep across USB, FIFO and buffer edges")
    full_init(link, args)
    rng = random.Random(args.seed)

    def read_sweep():
        total = 0
        for readcnt in BOUNDARY_SIZES:
            data = cmd_write_read(link, b"\x03\x00\x00\x00", readcnt, args.use_cs)
            if len(data) != readcnt:
                raise Failure("readcnt=%d returned %d bytes" % (readcnt, len(data)))
            link.expect_silence(0.02, "after readcnt=%d" % readcnt)
            total += readcnt
            runner.commands += 1
        return "%d read sizes, %d bytes" % (len(BOUNDARY_SIZES), total)

    def write_sweep():
        total = 0
        for writecnt in WRITE_SIZES:
            payload = bytes(rng.randrange(256) for _ in range(writecnt))
            data = cmd_write_read(link, payload, 0, args.use_cs)
            if len(data) != 0:
                raise Failure("writecnt=%d r=0 returned %d extra bytes" % (writecnt, len(data)))
            link.expect_silence(0.02, "after writecnt=%d" % writecnt)
            total += writecnt
            runner.commands += 1
        return "%d write sizes, %d bytes" % (len(WRITE_SIZES), total)

    def mixed_sweep():
        pairs = [(w, r) for w in (0, 1, 63, 64, 65, 128, 255, 256)
                 for r in (0, 1, 63, 64, 65, 128, 1024, 2048)]
        for writecnt, readcnt in pairs:
            if writecnt == 0 and readcnt == 0:
                continue
            payload = bytes(rng.randrange(256) for _ in range(writecnt))
            data = cmd_write_read(link, payload, readcnt, args.use_cs)
            if len(data) != readcnt:
                raise Failure("w=%d r=%d returned %d bytes" % (writecnt, readcnt, len(data)))
            runner.commands += 1
        link.expect_silence(0.05, "after mixed sweep")
        return "%d write/read combinations" % (len(pairs) - 1)

    def near_buffer_limit():
        for writecnt, readcnt in ((255, TMPBUFF_SIZE - 256),
                                  (0, TMPBUFF_SIZE),
                                  (TMPBUFF_SIZE - 1, 1)):
            payload = b"\x00" * writecnt
            data = cmd_write_read(link, payload, readcnt, args.use_cs)
            if len(data) != readcnt:
                raise Failure("w=%d r=%d returned %d bytes" % (writecnt, readcnt, len(data)))
            link.expect_silence(0.05, "after w=%d r=%d" % (writecnt, readcnt))
            runner.commands += 1
        return "w+r exactly at the %d byte buffer limit" % TMPBUFF_SIZE

    runner.run("read size sweep", read_sweep)
    runner.run("write size sweep", write_sweep)
    runner.run("mixed write/read sweep", mixed_sweep)
    runner.run("exact command buffer limit", near_buffer_limit)


def suite_timing(runner, args):
    link = runner.link
    print("\n[timing] fragmented, delayed and pipelined host traffic")
    full_init(link, args)
    rng = random.Random(args.seed + 1)

    def fragmented_header():
        readcnt = 512
        header = bytes([0x05, 0, 4, (readcnt >> 8) & 0xFF, readcnt & 0xFF])
        for byte in header:
            link.write(bytes([byte]))
            time.sleep(0.012)
        for byte in b"\x03\x00\x00\x00":
            link.write(bytes([byte]))
            time.sleep(0.012)
        resp = link.read_exact(1 + readcnt, "fragmented command")
        if resp[0] != ACK:
            raise Failure("fragmented command: ack was 0x%02x" % resp[0])
        link.expect_silence(0.05, "after fragmented command")
        runner.commands += 1
        return "header and payload split into 12ms spaced single bytes"

    def slow_payload():
        payload = bytes(rng.randrange(256) for _ in range(200))
        header = bytes([0x05, 0, 200, 0, 8])
        link.write(header)
        for offset in range(0, len(payload), 7):
            link.write(payload[offset:offset + 7])
            time.sleep(0.008)
        resp = link.read_exact(9, "slow payload")
        if resp[0] != ACK:
            raise Failure("slow payload: ack was 0x%02x" % resp[0])
        link.expect_silence(0.05, "after slow payload")
        runner.commands += 1
        return "200 byte payload dribbled in 7 byte chunks"

    def pipelined_burst():
        count = 24
        blob = bytearray()
        for _ in range(count):
            blob += bytes([0x05, 0, 4, 0, 12]) + b"\x03\x00\x00\x00"
        link.write(blob)
        resp = link.read_exact(count * 13, "pipelined burst")
        for index in range(count):
            if resp[index * 13] != ACK:
                raise Failure(
                    "pipelined burst: command %d ack was 0x%02x [%s]"
                    % (index, resp[index * 13], hexdump(resp[index * 13:index * 13 + 13]))
                )
        link.expect_silence(0.05, "after pipelined burst")
        runner.commands += count
        return "%d commands blasted in one write, %d bytes back" % (count, len(resp))

    def idle_resync():
        link.write(b"\x00")
        link.read_exact(len(BBIO_ID), "stray zero BBIO")
        link.write(b"\x00\x00\x00")
        link.expect_silence(0.10, "zeros inside the same burst")
        time.sleep(0.30)
        for _ in range(20):
            link.write(b"\x00")
        got = link.read_exact(len(BBIO_ID), "post-idle BBIO")
        if got != BBIO_ID:
            raise Failure("after an idle gap the resync burst must reply, got %r" % got)
        link.expect_silence(0.15, "after post-idle burst")
        full_init(link, args)
        return "stray zeros then 300ms idle then burst still replies"

    def interleaved_delays():
        for delay in (0.0, 0.001, 0.005, 0.020, 0.120, 0.400):
            time.sleep(delay)
            data = cmd_write_read(link, b"\x9f", 3, args.use_cs)
            if len(data) != 3:
                raise Failure("delay %.3fs: got %d bytes" % (delay, len(data)))
            runner.commands += 1
        return "commands after 0 to 400ms idle gaps"

    runner.run("fragmented single byte header", fragmented_header)
    runner.run("dribbled write payload", slow_payload)
    runner.run("pipelined command burst", pipelined_burst)
    runner.run("idle gap resynchronisation", idle_resync)
    runner.run("varied inter-command delays", interleaved_delays)


def suite_flashrom_pattern(runner, args):
    link = runner.link
    print("\n[flashrom] the exact command pattern of a real chip read")
    full_init(link, args)

    def chip_read_pattern():
        chunks = args.read_chunks
        address = 0
        total = 0
        runner.latencies = []
        for _ in range(chunks):
            header = bytes([0x03, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF])
            data = cmd_write_read(link, header, 2048, args.use_cs, runner)
            if len(data) != 2048:
                raise Failure("chunk at 0x%06x returned %d bytes" % (address, len(data)))
            address += 2048
            total += 2048
            runner.commands += 1
        latencies = sorted(runner.latencies)
        p50 = latencies[len(latencies) // 2]
        p99 = latencies[min(len(latencies) - 1, int(len(latencies) * 0.99))]
        return "%d x 2048B (%d kB), latency p50 %.1fms p99 %.1fms max %.1fms" % (
            chunks, total // 1024, p50, p99, latencies[-1]
        )

    def page_write_pattern():
        pages = min(args.read_chunks, 64)
        address = 0
        for _ in range(pages):
            payload = bytes([0x02, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF])
            payload += bytes(256)
            data = cmd_write_read(link, payload, 0, args.use_cs)
            if len(data) != 0:
                raise Failure("page write returned %d bytes" % len(data))
            address += 256
            runner.commands += 1
        return "%d x 260B page-program shaped writes" % pages

    runner.run("2048 byte read chunks", chip_read_pattern)
    runner.run("256 byte page write shape", page_write_pattern)


def suite_loopback(runner, args):
    link = runner.link
    print("\n[loopback] byte exact echo with IO7 jumpered to IO4")
    full_init(link, args)
    rng = random.Random(args.seed + 2)

    def echo_random():
        total = 0
        for _ in range(args.echo_iterations):
            length = rng.randrange(1, 17)
            payload = bytes(rng.randrange(256) for _ in range(length))
            got = cmd_bulk(link, payload)
            total += length
            runner.commands += 1
            if got != payload:
                raise Failure(
                    "echo mismatch after %d bytes\n        sent %s\n        got  %s"
                    % (total, hexdump(payload), hexdump(got))
                )
        return "%d transfers, %d bytes verified, 0 mismatches" % (args.echo_iterations, total)

    def echo_patterns():
        patterns = [
            b"\x00" * 16, b"\xff" * 16,
            bytes(range(16)), bytes(range(255, 239, -1)),
            b"\xaa\x55" * 8, b"\x55\xaa" * 8,
            b"\x01\x02\x04\x08\x10\x20\x40\x80" * 2,
            b"\xfe\xfd\xfb\xf7\xef\xdf\xbf\x7f" * 2,
            b"\x00\xff" * 8, b"\x0f\xf0" * 8,
        ]
        for payload in patterns:
            got = cmd_bulk(link, payload)
            runner.commands += 1
            if got != payload:
                raise Failure(
                    "pattern mismatch\n        sent %s\n        got  %s"
                    % (hexdump(payload), hexdump(got))
                )
        return "%d adversarial bit patterns" % len(patterns)

    def echo_every_byte_value():
        for base in range(0, 256, 16):
            payload = bytes(range(base, base + 16))
            got = cmd_bulk(link, payload)
            runner.commands += 1
            if got != payload:
                raise Failure(
                    "byte value sweep mismatch at 0x%02x\n        sent %s\n        got  %s"
                    % (base, hexdump(payload), hexdump(got))
                )
        return "all 256 byte values round trip intact"

    def read_phase_drives_ff():
        for readcnt in (64, 1024, 2048):
            data = cmd_write_read(link, b"\x03\x00\x00\x00", readcnt, args.use_cs)
            if len(data) != readcnt:
                raise Failure("expected %d bytes, got %d" % (readcnt, len(data)))
            bad = [i for i, b in enumerate(data) if b != 0xFF]
            if bad:
                raise Failure(
                    "read phase must drive 0xFF like a real BPv3; %d bad bytes, first at %d [%s]"
                    % (len(bad), bad[0], hexdump(data[max(0, bad[0] - 4):bad[0] + 12]))
                )
            runner.commands += 1
        return "64/1024/2048 byte reads are 0xFF end to end"

    runner.run("random 1-16 byte echo", echo_random)
    runner.run("adversarial bit patterns", echo_patterns)
    runner.run("all 256 byte values", echo_every_byte_value)
    runner.run("read phase drives 0xFF at scale", read_phase_drives_ff)


def suite_recovery(runner, args):
    link = runner.link
    print("\n[recovery] behaviour after an aborted or hostile host")
    rng = random.Random(args.seed + 4)

    def stall_midpayload():
        readcnt = 1024
        link.write(bytes([0x05, 0, 8, (readcnt >> 8) & 0xFF, readcnt & 0xFF]))
        link.write(b"\x03\x00\x00")
        time.sleep(0.05)
        link.write(b"\x00\x00\x00\x00\x00")
        link.read_exact(1 + readcnt, "completed after stall")
        link.expect_silence(0.05, "after stalled command")
        runner.commands += 1
        return "command completes after a mid-payload host stall"

    def truncated_then_resync():
        link.write(bytes([0x05, 0x00, 0x40]))
        time.sleep(0.20)
        link.write(bytes([0x00, 0x04]) + bytes(rng.randrange(256) for _ in range(64)))
        resp = link.read_exact(5, "truncated then completed")
        if resp[0] != ACK:
            raise Failure("expected ack, got 0x%02x" % resp[0])
        link.expect_silence(0.05, "after truncated command")
        runner.commands += 1
        return "split header plus 200ms pause still frames correctly"

    def abandoned_command():
        link.write(bytes([0x05, 0x00, 0x40, 0x00, 0x04]))
        link.write(b"\xde\xad\xbe\xef")
        time.sleep(PAYLOAD_TIMEOUT + 1.0)
        link.drain()
        bbio_reset(link, inactivity=6.0)
        full_init(link, args)
        cmd_write_read(link, b"\x9f", 3, args.use_cs)
        runner.commands += 1
        return "payload never completed, firmware abandoned it and resynced"

    def reopen_port():
        port = link.ser.port
        link.ser.close()
        time.sleep(0.35)
        link.ser.open()
        link.ser.dtr = True
        dropped = link.drain()
        full_init(link, args)
        data = cmd_write_read(link, b"\x9f", 3, args.use_cs)
        if len(data) != 3:
            raise Failure("after reopen: got %d bytes" % len(data))
        runner.commands += 1
        return "port %s closed, reopened, resynchronised (%d stale bytes)" % (port, dropped)

    def garbage_fuzz():
        rounds = args.fuzz_rounds
        sent = 0
        patient = 0
        for index in range(rounds):
            length = rng.randrange(8, 200)
            blob = bytes(rng.randrange(256) for _ in range(length))
            link.write(blob)
            sent += length
            time.sleep(PAYLOAD_TIMEOUT + 1.0)
            link.drain(quiet=0.4, cap=30.0)
            recovered = False
            for _ in range(4):
                try:
                    bbio_reset(link, inactivity=8.0)
                    recovered = True
                    break
                except Failure:
                    patient += 1
                    link.drain(quiet=0.4, cap=30.0)
            if not recovered:
                raise Failure("round %d never resynced after garbage" % index)
            full_init(link, args)
            data = cmd_write_read(link, b"\x9f", 3, args.use_cs)
            if len(data) != 3:
                raise Failure("round %d: bad reply after resync" % index)
            runner.commands += 1
        return "%d rounds, %d random bytes, always recovered (%d needed extra drain)" % (
            rounds, sent, patient
        )

    runner.run("stall in the middle of a payload", stall_midpayload)
    runner.run("truncated header then completion", truncated_then_resync)
    runner.run("abandoned payload is given up", abandoned_command)
    runner.run("close and reopen the port", reopen_port)
    if args.fuzz_rounds > 0:
        runner.run("random garbage then forced resync", garbage_fuzz)
    else:
        runner.skip("random garbage then forced resync", "--fuzz-rounds 0")


def suite_terminal(runner, args, term):
    link = runner.link
    print("\n[terminal] exit prompt on the user facing CDC port")

    def read_terminal(seconds, needle=None):
        deadline = time.monotonic() + seconds
        buf = bytearray()
        while time.monotonic() < deadline:
            chunk = term.read(256)
            if chunk:
                buf += chunk
                if needle and needle in bytes(buf):
                    break
        return bytes(buf)

    def q_then_no():
        term.reset_input_buffer()
        term.write(b"q")
        term.flush()
        text = read_terminal(2.0, b"(y/n)")
        if b"Exit legacy binary mode" not in text:
            raise Failure("no confirmation prompt after 'q', got %r" % text[-120:])
        term.write(b"n")
        term.flush()
        read_terminal(1.0)
        data = cmd_write_read(link, b"\x9f", 3, args.use_cs)
        if len(data) != 3:
            raise Failure("binary link broken after declining the exit")
        runner.commands += 1
        return "'q' prompts, 'n' declines, binary link unaffected"

    def q_then_timeout():
        term.reset_input_buffer()
        term.write(b"q")
        term.flush()
        text = read_terminal(2.0, b"(y/n)")
        if b"Exit legacy binary mode" not in text:
            raise Failure("no confirmation prompt after 'q'")
        text = read_terminal(18.0, b"staying in legacy")
        if b"staying in legacy" not in text:
            raise Failure("no answer should time out and continue, got %r" % text[-120:])
        data = cmd_write_read(link, b"\x9f", 3, args.use_cs)
        if len(data) != 3:
            raise Failure("binary link broken after the confirmation timeout")
        runner.commands += 1
        return "no answer times out after 15s and stays in the mode"

    def stray_keys_ignored():
        term.reset_input_buffer()
        term.write(b"hello world 12345\r\n")
        term.flush()
        time.sleep(0.3)
        data = cmd_write_read(link, b"\x9f", 3, args.use_cs)
        if len(data) != 3:
            raise Failure("stray terminal keys disturbed the binary link")
        runner.commands += 1
        return "non 'q' keystrokes are discarded harmlessly"

    def q_then_yes():
        term.reset_input_buffer()
        term.write(b"q")
        term.flush()
        text = read_terminal(2.0, b"(y/n)")
        if b"Exit legacy binary mode" not in text:
            raise Failure("no confirmation prompt after 'q'")
        term.write(b"y")
        term.flush()
        text = read_terminal(4.0, b"Resetting")
        if b"Exiting Legacy Binary Mode" not in text:
            raise Failure("exit was not announced, got %r" % text[-160:])
        return "'q' then 'y' exits and resets the Bus Pirate"

    runner.run("stray terminal keystrokes", stray_keys_ignored)
    runner.run("'q' then 'n' declines", q_then_no)
    runner.run("'q' with no answer times out", q_then_timeout)
    if args.exit_test:
        runner.run("'q' then 'y' exits the mode", q_then_yes)
    else:
        runner.skip("'q' then 'y' exits the mode", "--exit-test not given")


def suite_soak(runner, args):
    link = runner.link
    print("\n[soak] sustained random traffic for %d seconds" % args.soak)
    full_init(link, args)
    rng = random.Random(args.seed + 3)

    def soak():
        deadline = time.monotonic() + args.soak
        started = time.monotonic()
        commands = 0
        payload_bytes = 0
        while time.monotonic() < deadline:
            choice = rng.random()
            if args.loopback and choice < 0.35:
                length = rng.randrange(1, 17)
                payload = bytes(rng.randrange(256) for _ in range(length))
                got = cmd_bulk(link, payload)
                if got != payload:
                    raise Failure(
                        "soak echo mismatch after %d commands\n        sent %s\n        got  %s"
                        % (commands, hexdump(payload), hexdump(got))
                    )
                payload_bytes += length * 2
            else:
                writecnt = rng.choice([0, 1, 4, 16, 63, 64, 65, 127, 128, 256])
                readcnt = rng.choice([0, 1, 16, 63, 64, 65, 127, 128, 512, 1024, 2048])
                if writecnt == 0 and readcnt == 0:
                    readcnt = 1
                payload = bytes(rng.randrange(256) for _ in range(writecnt))
                data = cmd_write_read(link, payload, readcnt, args.use_cs)
                if len(data) != readcnt:
                    raise Failure(
                        "soak length mismatch after %d commands: w=%d r=%d got %d"
                        % (commands, writecnt, readcnt, len(data))
                    )
                if args.loopback and readcnt and any(b != 0xFF for b in data):
                    raise Failure(
                        "soak read phase content wrong after %d commands [%s]"
                        % (commands, hexdump(data))
                    )
                payload_bytes += writecnt + readcnt
            commands += 1
            runner.commands += 1
        elapsed = time.monotonic() - started
        rate = payload_bytes / elapsed if elapsed else 0
        return "%d commands, %d payload bytes, %.1f kB/s" % (
            commands, payload_bytes, rate / 1000.0
        )

    runner.run("random valid command soak", soak)


def enumerate_ports():
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    if not ports:
        print("no serial ports found")
        return
    print("%-10s %-30s %s" % ("PORT", "DESCRIPTION", "HWID"))
    for port in ports:
        print("%-10s %-30s %s" % (port.device, (port.description or "")[:30], port.hwid))
    print("\nThe Bus Pirate exposes two CDC interfaces. The binary one is the")
    print("second interface of the composite device (higher interface number).")


def main():
    parser = argparse.ArgumentParser(
        description="Conformance and USB robustness test for the Bus Pirate legacy binary mode",
        epilog=EPILOG,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--port", help="binary CDC port of the Bus Pirate, e.g. COM19")
    parser.add_argument("--terminal", help="user facing CDC port, enables the exit prompt tests")
    parser.add_argument("--list", action="store_true", help="list serial ports and exit")
    parser.add_argument("--loopback", action="store_true",
                        help="IO7 (MOSI) is jumpered to IO4 (MISO); enables echo verification")
    parser.add_argument("--use-cs", action="store_true",
                        help="use opcode 0x04 (asserts CS#) instead of 0x05; nothing attached only")
    parser.add_argument("--psu", action="store_true",
                        help="enable the Bus Pirate power supply during the test")
    parser.add_argument("--spispeed", type=int, default=7, choices=range(8), metavar="0-7",
                        help="SPI speed index sent with 0x60 (default 7, the flashrom default)")
    parser.add_argument("--soak", type=int, default=30, metavar="SECONDS",
                        help="duration of the random traffic soak (0 disables, default 30)")
    parser.add_argument("--echo-iterations", type=int, default=4000, metavar="N",
                        help="loopback echo transfers (default 4000)")
    parser.add_argument("--read-chunks", type=int, default=200, metavar="N",
                        help="2048 byte read chunks in the flashrom pattern (default 200)")
    parser.add_argument("--fuzz-rounds", type=int, default=6, metavar="N",
                        help="random garbage recovery rounds, 0 disables (default 6)")
    parser.add_argument("--reentry", type=int, default=8, metavar="N",
                        help="mode re-entry cycles (default 8)")
    parser.add_argument("--exit-test", action="store_true",
                        help="also confirm 'q' then 'y', which exits the mode and resets the board")
    parser.add_argument("--seed", type=int, default=1, help="random seed (default 1)")
    parser.add_argument("--inactivity-timeout", type=float, default=3.0, metavar="SECONDS",
                        help="fail if no byte arrives for this long (default 3.0)")
    parser.add_argument("--hard-timeout", type=float, default=120.0, metavar="SECONDS",
                        help="fail if a single response takes longer than this (default 120)")
    parser.add_argument("--quick", action="store_true",
                        help="short run: no soak, fewer echoes, fewer chunks, no fuzz")
    args = parser.parse_args()

    if args.list:
        enumerate_ports()
        return 0
    if not args.port:
        parser.error("--port is required (use --list to find it)")

    if args.quick:
        args.soak = 0
        args.echo_iterations = min(args.echo_iterations, 400)
        args.read_chunks = min(args.read_chunks, 20)
        args.reentry = min(args.reentry, 3)
        args.fuzz_rounds = 0

    print("Bus Pirate legacy binary mode - conformance and robustness test")
    print("  binary port    %s" % args.port)
    print("  terminal port  %s" % (args.terminal or "not given, exit prompt tests skipped"))
    print("  loopback       %s" % ("yes (IO7 -> IO4)" if args.loopback else "no (framing only)"))
    print("  chip select    %s" % ("0x04, CS# asserted" if args.use_cs else "0x05, CS# never asserted"))
    print("  power supply   %s" % ("on" if args.psu else "off"))
    print("  spi speed idx  %d" % args.spispeed)
    if args.use_cs:
        print("\n  WARNING: --use-cs drives real selected transactions at a connected chip.")
    if args.fuzz_rounds:
        print("  WARNING: the fuzzer sends random opcodes and can toggle the supply and pins.")

    try:
        link = Link(args.port, args.inactivity_timeout, args.hard_timeout)
    except serial.SerialException as exc:
        print("\ncannot open %s: %s" % (args.port, exc))
        return 2

    term = None
    if args.terminal:
        try:
            term = serial.Serial(args.terminal, 115200, timeout=0.05, write_timeout=5.0)
            term.dtr = True
        except serial.SerialException as exc:
            print("\ncannot open terminal port %s: %s" % (args.terminal, exc))
            link.close()
            return 2

    runner = Runner(link, args)
    started = time.monotonic()

    try:
        stale = link.drain()
        if stale:
            print("\n  drained %d stale bytes before starting" % stale)
        suite_handshake(runner, args)
        suite_layers(runner, args)
        suite_framing(runner, args)
        suite_avr(runner, args)
        suite_sizes(runner, args)
        suite_timing(runner, args)
        suite_flashrom_pattern(runner, args)
        if args.loopback:
            suite_loopback(runner, args)
        else:
            print("\n[loopback] skipped, pass --loopback with IO7 jumpered to IO4")
        suite_recovery(runner, args)
        if term is not None:
            suite_terminal(runner, args, term)
        else:
            print("\n[terminal] skipped, pass --terminal COMx")
        if args.soak > 0:
            suite_soak(runner, args)
        else:
            print("\n[soak] skipped")
    except KeyboardInterrupt:
        print("\ninterrupted")
    finally:
        if not args.exit_test:
            try:
                link.drain()
                bbio_reset(link)
            except Exception:
                pass
        elapsed = time.monotonic() - started

    print("\n" + "=" * 72)
    print("commands issued   %d" % runner.commands)
    print("bytes to device   %d" % link.tx_bytes)
    print("bytes from device %d" % link.rx_bytes)
    if runner.latencies:
        ordered = sorted(runner.latencies)
        print("2048B read latency p50 %.1fms  p99 %.1fms  max %.1fms  mean %.1fms" % (
            ordered[len(ordered) // 2],
            ordered[min(len(ordered) - 1, int(len(ordered) * 0.99))],
            ordered[-1],
            statistics.fmean(ordered),
        ))
    print("elapsed           %.1fs" % elapsed)
    print("tests passed      %d" % runner.passed)
    print("tests skipped     %d" % runner.skipped)
    print("tests failed      %d" % runner.failed)
    if runner.failures:
        print("\nFAILURES")
        for name, detail in runner.failures:
            print("  %s" % name)
            print("    %s" % detail)
        print("\nRESULT: FAIL")
    else:
        print("\nRESULT: PASS - no packet loss, no framing error, no stall")
    print("=" * 72)

    link.close()
    if term is not None:
        term.close()
    return 1 if runner.failed else 0


if __name__ == "__main__":
    sys.exit(main())
