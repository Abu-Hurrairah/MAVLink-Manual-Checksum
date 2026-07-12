#!/usr/bin/env python3
"""
Manual MAVLink checksum calculator and verifier.

Supports MAVLink v1 (0xFE) and MAVLink v2 (0xFD) frames without importing
pymavlink or the official MAVLink C/Python libraries.

Usage:
    python mavlink_checksum.py FD130000C301014A00002E029A4184A0F641F6DE8E4549A1BCBF2A01329B87 20
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass

MAVLINK_V1_MAGIC = 0xFE
MAVLINK_V2_MAGIC = 0xFD
MAVLINK_V2_IFLAG_SIGNED = 0x01
X25_INIT_CRC = 0xFFFF


@dataclass(frozen=True)
class MavlinkChecksumResult:
    version: int
    payload_length: int
    message_id: int
    crc_extra: int
    calculated_crc: int
    calculated_lsb: int
    calculated_msb: int
    provided_crc: int | None
    valid: bool | None


def clean_hex_string(hex_string: str) -> str:
    """Remove spaces, commas, 0x prefixes, and line breaks from a hex string."""
    cleaned = (
        hex_string.replace("0x", "")
        .replace("0X", "")
        .replace(" ", "")
        .replace(",", "")
        .replace("\n", "")
        .replace("\r", "")
        .replace("\t", "")
    )
    if len(cleaned) % 2 != 0:
        raise ValueError("Hex string must contain an even number of hex characters.")
    return cleaned


def bytes_from_hex(hex_string: str) -> bytes:
    """Convert a flexible hex string into bytes."""
    try:
        return bytes.fromhex(clean_hex_string(hex_string))
    except ValueError as exc:
        raise ValueError("Invalid hex string. Use bytes like FD 13 00 ...") from exc


def x25_crc_accumulate(byte: int, crc: int) -> int:
    """Accumulate one byte using the MAVLink X.25 / CRC-16-MCRF4XX algorithm."""
    tmp = byte ^ (crc & 0xFF)
    tmp = (tmp ^ (tmp << 4)) & 0xFF
    crc = ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    return crc


def x25_crc(data: bytes, crc_extra: int | None = None) -> int:
    """Calculate MAVLink X.25 CRC over data and optionally accumulate CRC_EXTRA."""
    crc = X25_INIT_CRC
    for byte in data:
        crc = x25_crc_accumulate(byte, crc)
    if crc_extra is not None:
        if not 0 <= crc_extra <= 255:
            raise ValueError("crc_extra must be between 0 and 255.")
        crc = x25_crc_accumulate(crc_extra, crc)
    return crc


def parse_mavlink_frame(frame: bytes) -> tuple[int, int, int, int, int, int]:
    """
    Parse a MAVLink frame.

    Returns:
        version, payload_length, message_id, crc_input_start, checksum_offset, expected_length
    """
    if len(frame) < 8:
        raise ValueError("Frame is too short to be a MAVLink packet.")

    magic = frame[0]

    if magic == MAVLINK_V1_MAGIC:
        payload_length = frame[1]
        header_length = 6
        checksum_offset = header_length + payload_length
        expected_length = checksum_offset + 2
        if len(frame) != expected_length:
            raise ValueError(
                f"Invalid MAVLink v1 frame length. Expected {expected_length} bytes, got {len(frame)}."
            )
        message_id = frame[5]
        return 1, payload_length, message_id, 1, checksum_offset, expected_length

    if magic == MAVLINK_V2_MAGIC:
        if len(frame) < 12:
            raise ValueError("Frame is too short to be a MAVLink v2 packet.")
        payload_length = frame[1]
        header_length = 10
        signed = bool(frame[2] & MAVLINK_V2_IFLAG_SIGNED)
        checksum_offset = header_length + payload_length
        expected_length = checksum_offset + 2 + (13 if signed else 0)
        if len(frame) != expected_length:
            raise ValueError(
                f"Invalid MAVLink v2 frame length. Expected {expected_length} bytes, got {len(frame)}."
            )
        message_id = frame[7] | (frame[8] << 8) | (frame[9] << 16)
        return 2, payload_length, message_id, 1, checksum_offset, expected_length

    raise ValueError(f"Unsupported MAVLink magic byte 0x{magic:02X}. Expected 0xFE or 0xFD.")


def calculate_frame_checksum(frame: bytes, crc_extra: int) -> MavlinkChecksumResult:
    """Calculate and verify the checksum for a complete MAVLink frame."""
    version, payload_length, message_id, crc_input_start, checksum_offset, _ = parse_mavlink_frame(frame)

    # MAVLink frame checksum is calculated over the serialized packet excluding:
    # - magic/start byte
    # - checksum bytes
    # - optional MAVLink 2 signature bytes
    crc_input = frame[crc_input_start:checksum_offset]
    calculated_crc = x25_crc(crc_input, crc_extra)
    provided_crc = frame[checksum_offset] | (frame[checksum_offset + 1] << 8)

    return MavlinkChecksumResult(
        version=version,
        payload_length=payload_length,
        message_id=message_id,
        crc_extra=crc_extra,
        calculated_crc=calculated_crc,
        calculated_lsb=calculated_crc & 0xFF,
        calculated_msb=(calculated_crc >> 8) & 0xFF,
        provided_crc=provided_crc,
        valid=(calculated_crc == provided_crc),
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Manual MAVLink checksum calculator/verifier")
    parser.add_argument("frame_hex", help="Complete MAVLink frame as hex string")
    parser.add_argument("crc_extra", type=int, help="CRC_EXTRA value for the message type, e.g. 20 for VFR_HUD")
    args = parser.parse_args()

    frame = bytes_from_hex(args.frame_hex)
    result = calculate_frame_checksum(frame, args.crc_extra)

    print(f"MAVLink version : v{result.version}")
    print(f"Message ID      : {result.message_id}")
    print(f"Payload length  : {result.payload_length}")
    print(f"CRC_EXTRA       : {result.crc_extra}")
    print(f"Calculated CRC  : 0x{result.calculated_crc:04X}")
    print(f"Checksum bytes  : 0x{result.calculated_lsb:02X} 0x{result.calculated_msb:02X}  (little-endian)")
    print(f"Provided CRC    : 0x{result.provided_crc:04X}")
    print(f"Valid           : {result.valid}")


if __name__ == "__main__":
    main()
