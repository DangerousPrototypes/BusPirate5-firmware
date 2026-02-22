"""
bp_bpio_client.py — Bus Pirate BPIO2 FlatBuffers/COBS binary client (CDC 1).

Wire protocol:
  TX: FlatBuffers RequestPacket → COBS-encode → append 0x00 → write
  RX: read until 0x00 → COBS-decode → parse FlatBuffers ResponsePacket

Uses the ``flatbuffers`` Python package (google-flatbuffers) with manual
builder calls to avoid requiring a ``flatc`` code-generation step.
Uses the ``cobs`` PyPI package for COBS encode/decode.
"""
# Copyright (c) 2024 Ian Lesnet
# Modified by contributors to the BusPirate5-firmware project

import struct
import time

import serial
from cobs import cobs
import flatbuffers
from flatbuffers import builder as flatbuffers_builder

# ---------------------------------------------------------------------------
# FlatBuffers field constants (from bpio.fbs)
# Table field IDs follow FlatBuffers vtable layout rules:
#   field index N lives at vtable offset (4 + 2*N).
# All offsets below are byte offsets into the vtable from the table start.
#
# RequestPacket fields (indices 0,1,2):
#   0: version_major     (uint8)
#   1: minimum_version_minor (uint16)
#   2: contents          (union — two fields: type tag at idx 2, value at idx 3)
#
# Union contents type IDs (RequestPacketContents):
#   1 = StatusRequest
#   2 = ConfigurationRequest
#   3 = DataRequest
#
# ResponsePacket fields (indices 0,1):
#   0: error             (string)
#   1: contents          (union — type tag at idx 1, value at idx 2)
#
# Union contents type IDs (ResponsePacketContents):
#   1 = StatusResponse
#   2 = ConfigurationResponse
#   3 = DataResponse
# ---------------------------------------------------------------------------

_BPIO_VERSION_MAJOR = 2
_BPIO_VERSION_MINOR = 0

_UNION_STATUS_REQUEST = 1
_UNION_CONFIG_REQUEST = 2

_UNION_STATUS_RESPONSE = 1
_UNION_CONFIG_RESPONSE = 2

_MAX_PACKET_SIZE = 640


# ---------------------------------------------------------------------------
# Low-level FlatBuffers helpers
# ---------------------------------------------------------------------------

def _encode_request(builder: flatbuffers.Builder, contents_type: int, contents_offset: int) -> bytes:
    """Finish a RequestPacket and return the serialized bytes."""
    # Build RequestPacket table
    builder.StartObject(4)  # 4 fields: version_major, min_version_minor, contents_type, contents_value
    builder.PrependUint8Slot(0, _BPIO_VERSION_MAJOR, 0)       # version_major
    builder.PrependUint16Slot(1, _BPIO_VERSION_MINOR, 0)      # minimum_version_minor
    builder.PrependUint8Slot(2, contents_type, 0)             # contents type tag (union)
    builder.PrependUOffsetTRelativeSlot(3, contents_offset, 0)  # contents value
    packet = builder.EndObject()
    builder.Finish(packet)
    return bytes(builder.Output())


def _build_status_request(queries: list[int] | None = None) -> bytes:
    """Build a StatusRequest packet (all queries if *queries* is None)."""
    builder = flatbuffers.Builder(_MAX_PACKET_SIZE)

    # Build optional query vector
    query_vec = None
    if queries:
        builder.StartVector(1, len(queries), 1)
        for q in reversed(queries):
            builder.PrependByte(q)
        query_vec = builder.EndVector(len(queries))

    # Build StatusRequest table (1 field: query vector)
    builder.StartObject(1)
    if query_vec is not None:
        builder.PrependUOffsetTRelativeSlot(0, query_vec, 0)
    status_req = builder.EndObject()

    return _encode_request(builder, _UNION_STATUS_REQUEST, status_req)


def _build_configuration_request(**kwargs) -> bytes:
    """Build a ConfigurationRequest packet with the given keyword arguments.

    Supported kwargs mirror the FlatBuffers schema fields:
      mode (str), psu_enable (bool), psu_disable (bool),
      psu_set_mv (int), psu_set_ma (int),
      pullup_enable (bool), pullup_disable (bool),
      hardware_bootloader (bool), hardware_reset (bool),
      hardware_selftest (bool), print_string (str), ...
    """
    builder = flatbuffers.Builder(_MAX_PACKET_SIZE)

    # Pre-build string offsets (must be created before StartObject)
    string_offsets: dict[str, int] = {}
    if "mode" in kwargs:
        string_offsets["mode"] = builder.CreateString(kwargs["mode"])
    if "print_string" in kwargs:
        string_offsets["print_string"] = builder.CreateString(kwargs["print_string"])

    # ConfigurationRequest field indices (from bpio.fbs order):
    #  0: mode (string)
    #  1: mode_configuration (table) — not implemented here
    #  2: mode_bitorder_msb (bool)
    #  3: mode_bitorder_lsb (bool)
    #  4: psu_disable (bool)
    #  5: psu_enable (bool)
    #  6: psu_set_mv (uint32)
    #  7: psu_set_ma (uint16)
    #  8: pullup_disable (bool)
    #  9: pullup_enable (bool)
    # 10: io_direction_mask (uint8)
    # 11: io_direction (uint8)
    # 12: io_value_mask (uint8)
    # 13: io_value (uint8)
    # 14: led_resume (bool)
    # 15: led_color (vector) — not implemented here
    # 16: print_string (string)
    # 17: hardware_bootloader (bool)
    # 18: hardware_reset (bool)
    # 19: hardware_selftest (bool)

    builder.StartObject(20)

    if "mode" in string_offsets:
        builder.PrependUOffsetTRelativeSlot(0, string_offsets["mode"], 0)
    if kwargs.get("mode_bitorder_msb"):
        builder.PrependBoolSlot(2, True, False)
    if kwargs.get("mode_bitorder_lsb"):
        builder.PrependBoolSlot(3, True, False)
    if kwargs.get("psu_disable"):
        builder.PrependBoolSlot(4, True, False)
    if kwargs.get("psu_enable"):
        builder.PrependBoolSlot(5, True, False)
    if "psu_set_mv" in kwargs:
        builder.PrependUint32Slot(6, int(kwargs["psu_set_mv"]), 0)
    if "psu_set_ma" in kwargs:
        builder.PrependUint16Slot(7, int(kwargs["psu_set_ma"]), 300)
    if kwargs.get("pullup_disable"):
        builder.PrependBoolSlot(8, True, False)
    if kwargs.get("pullup_enable"):
        builder.PrependBoolSlot(9, True, False)
    if "io_direction_mask" in kwargs:
        builder.PrependUint8Slot(10, int(kwargs["io_direction_mask"]), 0)
    if "io_direction" in kwargs:
        builder.PrependUint8Slot(11, int(kwargs["io_direction"]), 0)
    if "io_value_mask" in kwargs:
        builder.PrependUint8Slot(12, int(kwargs["io_value_mask"]), 0)
    if "io_value" in kwargs:
        builder.PrependUint8Slot(13, int(kwargs["io_value"]), 0)
    if kwargs.get("led_resume"):
        builder.PrependBoolSlot(14, True, False)
    if "print_string" in string_offsets:
        builder.PrependUOffsetTRelativeSlot(16, string_offsets["print_string"], 0)
    if kwargs.get("hardware_bootloader"):
        builder.PrependBoolSlot(17, True, False)
    if kwargs.get("hardware_reset"):
        builder.PrependBoolSlot(18, True, False)
    if kwargs.get("hardware_selftest"):
        builder.PrependBoolSlot(19, True, False)

    config_req = builder.EndObject()
    return _encode_request(builder, _UNION_CONFIG_REQUEST, config_req)


# ---------------------------------------------------------------------------
# Low-level FlatBuffers read helpers
# ---------------------------------------------------------------------------

def _read_string(buf: bytes, offset: int) -> str | None:
    """Read a FlatBuffers string at absolute *offset* in *buf*."""
    if offset == 0:
        return None
    length = struct.unpack_from("<I", buf, offset)[0]
    return buf[offset + 4: offset + 4 + length].decode("utf-8", errors="replace")


def _follow_offset(buf: bytes, pos: int) -> int:
    """Follow a relative offset field at *pos*, return absolute position."""
    rel = struct.unpack_from("<i", buf, pos)[0]
    return pos + rel


def _read_table_field(buf: bytes, table_pos: int, field_index: int) -> int | None:
    """Return the absolute byte position of field *field_index* in a table,
    or ``None`` if the field is not present."""
    vtable_offset = struct.unpack_from("<i", buf, table_pos)[0]
    vtable_pos = table_pos - vtable_offset
    vtable_size = struct.unpack_from("<H", buf, vtable_pos)[0]
    field_offset_pos = vtable_pos + 4 + field_index * 2
    if field_offset_pos + 2 > vtable_pos + vtable_size:
        return None
    field_offset = struct.unpack_from("<H", buf, field_offset_pos)[0]
    if field_offset == 0:
        return None
    return table_pos + field_offset


def _read_scalar(buf: bytes, pos: int, fmt: str):
    """Read a scalar value at absolute *pos* using struct format *fmt*."""
    return struct.unpack_from(fmt, buf, pos)[0]


def _parse_response_packet(buf: bytes) -> dict:
    """Parse a FlatBuffers ResponsePacket bytes object into a plain dict.

    Returns a dict with at minimum:
      ``error`` (str or None), ``contents_type`` (int), ``contents`` (dict)
    """
    # Root table offset is stored at byte 0 as a UOffset
    root_offset = struct.unpack_from("<I", buf, 0)[0]
    table_pos = root_offset  # absolute position of ResponsePacket table

    result = {"error": None, "contents_type": 0, "contents": {}}

    # Field 0: error (string)
    err_pos = _read_table_field(buf, table_pos, 0)
    if err_pos is not None:
        str_abs = _follow_offset(buf, err_pos)
        result["error"] = _read_string(buf, str_abs)

    # Field 1: contents union type tag (uint8)
    type_pos = _read_table_field(buf, table_pos, 1)
    if type_pos is None:
        return result
    contents_type = _read_scalar(buf, type_pos, "<B")
    result["contents_type"] = contents_type

    # Field 2: contents union value (offset to table)
    val_pos = _read_table_field(buf, table_pos, 2)
    if val_pos is None:
        return result
    contents_table_pos = _follow_offset(buf, val_pos)

    if contents_type == _UNION_STATUS_RESPONSE:
        result["contents"] = _parse_status_response(buf, contents_table_pos)
    elif contents_type == _UNION_CONFIG_RESPONSE:
        result["contents"] = _parse_config_response(buf, contents_table_pos)
    else:
        result["contents"] = {}

    return result


def _parse_status_response(buf: bytes, table_pos: int) -> dict:
    """Parse a StatusResponse table into a plain dict."""

    def str_field(idx):
        pos = _read_table_field(buf, table_pos, idx)
        if pos is None:
            return None
        return _read_string(buf, _follow_offset(buf, pos))

    def uint8_field(idx):
        pos = _read_table_field(buf, table_pos, idx)
        return _read_scalar(buf, pos, "<B") if pos is not None else None

    def uint16_field(idx):
        pos = _read_table_field(buf, table_pos, idx)
        return _read_scalar(buf, pos, "<H") if pos is not None else None

    def uint32_field(idx):
        pos = _read_table_field(buf, table_pos, idx)
        return _read_scalar(buf, pos, "<I") if pos is not None else None

    def bool_field(idx):
        pos = _read_table_field(buf, table_pos, idx)
        return bool(_read_scalar(buf, pos, "<B")) if pos is not None else None

    def float_field(idx):
        pos = _read_table_field(buf, table_pos, idx)
        return _read_scalar(buf, pos, "<f") if pos is not None else None

    def str_vec_field(idx):
        pos = _read_table_field(buf, table_pos, idx)
        if pos is None:
            return []
        vec_abs = _follow_offset(buf, pos)
        count = struct.unpack_from("<I", buf, vec_abs)[0]
        items = []
        for i in range(count):
            elem_pos = vec_abs + 4 + i * 4
            elem_abs = _follow_offset(buf, elem_pos)
            items.append(_read_string(buf, elem_abs))
        return items

    # StatusResponse field index mapping (from bpio.fbs):
    #  0: error (string)
    #  1: version_flatbuffers_major (uint8)
    #  2: version_flatbuffers_minor (uint16)
    #  3: version_hardware_major (uint8)
    #  4: version_hardware_minor (uint8)
    #  5: version_firmware_major (uint8)
    #  6: version_firmware_minor (uint8)
    #  7: version_firmware_git_hash (string)
    #  8: version_firmware_date (string)
    #  9: modes_available ([string])
    # 10: mode_current (string)
    # 11: mode_pin_labels ([string])
    # 12: mode_bitorder_msb (bool)
    # 13: mode_max_packet_size (uint32)
    # 14: mode_max_write (uint32)
    # 15: mode_max_read (uint32)
    # 16: psu_enabled (bool)
    # 17: psu_set_mv (uint32)
    # 18: psu_set_ma (uint32)
    # 19: psu_measured_mv (uint32)
    # 20: psu_measured_ma (uint32)
    # 21: psu_current_error (bool)
    # 22: pullup_enabled (bool)
    # 23: adc_mv ([uint32])
    # 24: io_direction (uint8)
    # 25: io_value (uint8)
    # 26: disk_size_mb (float)
    # 27: disk_used_mb (float)
    # 28: led_count (uint8)
    return {
        "error": str_field(0),
        "version_flatbuffers_major": uint8_field(1),
        "version_flatbuffers_minor": uint16_field(2),
        "version_hardware_major": uint8_field(3),
        "version_hardware_minor": uint8_field(4),
        "version_firmware_major": uint8_field(5),
        "version_firmware_minor": uint8_field(6),
        "version_firmware_git_hash": str_field(7),
        "version_firmware_date": str_field(8),
        "modes_available": str_vec_field(9),
        "mode_current": str_field(10),
        "mode_pin_labels": str_vec_field(11),
        "mode_bitorder_msb": bool_field(12),
        "mode_max_packet_size": uint32_field(13),
        "mode_max_write": uint32_field(14),
        "mode_max_read": uint32_field(15),
        "psu_enabled": bool_field(16),
        "psu_set_mv": uint32_field(17),
        "psu_set_ma": uint32_field(18),
        "psu_measured_mv": uint32_field(19),
        "psu_measured_ma": uint32_field(20),
        "psu_current_error": bool_field(21),
        "pullup_enabled": bool_field(22),
        "io_direction": uint8_field(24),
        "io_value": uint8_field(25),
        "disk_size_mb": float_field(26),
        "disk_used_mb": float_field(27),
        "led_count": uint8_field(28),
    }


def _parse_config_response(buf: bytes, table_pos: int) -> dict:
    """Parse a ConfigurationResponse table into a plain dict."""
    pos = _read_table_field(buf, table_pos, 0)
    error = None
    if pos is not None:
        error = _read_string(buf, _follow_offset(buf, pos))
    return {"error": error}


# ---------------------------------------------------------------------------
# BPIOClient
# ---------------------------------------------------------------------------

class BPIOClient:
    """Client for the Bus Pirate BPIO2 binary protocol (CDC 1).

    Parameters
    ----------
    port:
        Serial device path for CDC 1 (e.g. ``/dev/ttyACM1``).
    baudrate:
        Baud rate (CDC ACM ignores this but pyserial requires it).
    timeout:
        Per-read timeout in seconds (firmware expects reply within 500 ms).
    debug:
        Print raw packet bytes when ``True``.
    """

    def __init__(
        self,
        port: str = "/dev/ttyACM1",
        baudrate: int = 115200,
        timeout: float = 1.0,
        debug: bool = False,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.debug = debug
        self._ser: serial.Serial | None = None
        self.connect()

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def connect(self) -> None:
        """Open the serial port."""
        self._ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)

    def close(self) -> None:
        """Close the serial port."""
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    # ------------------------------------------------------------------
    # Wire I/O
    # ------------------------------------------------------------------

    def _send(self, payload: bytes) -> None:
        """COBS-encode *payload*, append 0x00 delimiter, and write."""
        frame = cobs.encode(payload) + b"\x00"
        if self.debug:
            print(f"[BPIO TX {len(frame)} bytes] {frame.hex()}")
        self._ser.write(frame)

    def _recv(self) -> bytes:
        """Read bytes until 0x00 delimiter, COBS-decode, and return payload."""
        buf = bytearray()
        deadline = time.time() + 5.0  # hard 5-second ceiling
        while time.time() < deadline:
            b = self._ser.read(1)
            if not b:
                continue
            if b == b"\x00":
                break
            buf.extend(b)
        else:
            raise TimeoutError("Timed out waiting for BPIO response delimiter")

        if not buf:
            raise RuntimeError("Received empty BPIO frame")

        decoded = cobs.decode(bytes(buf))
        if self.debug:
            print(f"[BPIO RX {len(decoded)} bytes] {decoded.hex()}")
        return decoded

    def _transact(self, payload: bytes) -> dict:
        """Send a request and parse the response. Raise on protocol errors."""
        self._send(payload)
        raw = self._recv()
        response = _parse_response_packet(raw)

        top_error = response.get("error")
        if top_error:
            raise RuntimeError(f"BPIO protocol error: {top_error}")

        contents_error = response.get("contents", {}).get("error")
        if contents_error:
            raise RuntimeError(f"BPIO response error: {contents_error}")

        return response

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def get_status(self, queries: list[int] | None = None) -> dict:
        """Send a StatusRequest and return the StatusResponse as a dict.

        Parameters
        ----------
        queries:
            List of ``StatusRequestTypes`` enum values to query, or ``None``
            to request all status information.
        """
        payload = _build_status_request(queries)
        response = self._transact(payload)
        return response.get("contents", {})

    def send_configuration(self, **kwargs) -> dict:
        """Send a ConfigurationRequest and return the ConfigurationResponse."""
        payload = _build_configuration_request(**kwargs)
        response = self._transact(payload)
        return response.get("contents", {})

    def enter_bootloader(self) -> None:
        """Send ConfigurationRequest with hardware_bootloader=True, then close."""
        payload = _build_configuration_request(hardware_bootloader=True)
        self._send(payload)
        # Don't wait for a response — the device reboots immediately
        time.sleep(0.1)
        self.close()

    def reset(self) -> None:
        """Send ConfigurationRequest with hardware_reset=True, then close."""
        payload = _build_configuration_request(hardware_reset=True)
        self._send(payload)
        time.sleep(0.1)
        self.close()
