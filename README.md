# MAVLink Manual Checksum Calculator

A lightweight implementation for calculating and verifying MAVLink packet checksums without using the official MAVLink library.

This project is useful for embedded systems, microcontrollers, and low-memory environments where importing the full MAVLink library may be unnecessary or too heavy. It includes implementations in **Python**, **C**, and an **Arduino-compatible sketch**.

---

## Overview

MAVLink packets include a 2-byte checksum that helps verify whether a packet was transmitted correctly and whether the sender and receiver agree on the message definition. The checksum is calculated using the MAVLink X.25 / CRC-16-MCRF4XX algorithm and a message-specific `CRC_EXTRA` value.

This repository demonstrates how to:

- Parse MAVLink v1 and MAVLink v2 frames
- Remove the start byte from checksum input
- Exclude checksum bytes and optional MAVLink 2 signature bytes
- Accumulate packet bytes using the X.25 CRC algorithm
- Add the message-specific `CRC_EXTRA` byte
- Compare calculated checksum with the checksum inside the MAVLink frame
- Output checksum bytes in little-endian order

---

## Why This Project Exists

The official MAVLink libraries are powerful, but on small embedded boards they can consume more memory than needed for simple packet verification. For example, Arduino UNO-class boards have very limited SRAM, so a small custom checksum function can be useful when only basic MAVLink validation is required.

This project is not a replacement for full MAVLink parsing. It is focused specifically on **manual checksum calculation and verification**.

---

## Project Structure

```text
mavlink-manual-checksum/
│
├── src/
│   ├── mavlink_checksum.py      # Python implementation
│   └── mavlink_checksum.c       # Standard C implementation
│
├── arduino/
│   └── MAVLinkChecksum/
│       └── MAVLinkChecksum.ino  # Arduino-compatible example
│
├── examples/
│   └── example_frame.txt        # Test frame and expected checksum
│
├── REPORT.md                    # Professional technical report
└── README.md                    # Project documentation
```

---

## Example Frame

The included example uses a MAVLink v2 packet:

```text
FD130000C301014A00002E029A4184A0F641F6DE8E4549A1BCBF2A01329B87
```

For this frame:

| Field | Value |
|---|---|
| MAVLink version | v2 |
| Start byte | `0xFD` |
| Payload length | `0x13` / 19 bytes |
| Message ID | `74` / `VFR_HUD` |
| CRC_EXTRA | `20` |
| Provided checksum bytes | `9B 87` |
| Provided checksum value | `0x879B` |

Expected result:

```text
Calculated CRC: 0x879B
Checksum bytes: 9B 87
Valid: true
```

---

## Python Usage

Run the Python implementation:

```bash
python src/mavlink_checksum.py FD130000C301014A00002E029A4184A0F641F6DE8E4549A1BCBF2A01329B87 20
```

Expected output:

```text
MAVLink version : v2
Message ID      : 74
Payload length  : 19
CRC_EXTRA       : 20
Calculated CRC  : 0x879B
Checksum bytes  : 0x9B 0x87  (little-endian)
Provided CRC    : 0x879B
Valid           : True
```

No external Python packages are required.

---

## C Usage

Compile the C implementation:

```bash
gcc -Wall -Wextra -std=c99 src/mavlink_checksum.c -o mavlink_checksum
```

Run it:

```bash
./mavlink_checksum FD130000C301014A00002E029A4184A0F641F6DE8E4549A1BCBF2A01329B87 20
```

Expected output:

```text
MAVLink version : v2
Message ID      : 74
Payload length  : 19
CRC_EXTRA       : 20
Calculated CRC  : 0x879B
Checksum bytes  : 0x9B 0x87  (little-endian)
Provided CRC    : 0x879B
Valid           : true
```

---

## Arduino Usage

Open this sketch in the Arduino IDE:

```text
arduino/MAVLinkChecksum/MAVLinkChecksum.ino
```

Upload it to the board and open the Serial Monitor at `9600` baud.

Expected Serial Monitor output:

```text
Calculated CRC: 0x879B
Provided CRC:   0x879B
Valid:          true
```

---

## Important MAVLink Checksum Rule

For a complete MAVLink packet, the checksum is calculated over the serialized packet **excluding**:

- The start byte / magic byte (`0xFE` or `0xFD`)
- The 2 checksum bytes
- The optional MAVLink 2 signature bytes

Then the message-specific `CRC_EXTRA` byte is accumulated at the end.

For MAVLink v2, the packet header is:

```text
magic, len, incompat_flags, compat_flags, seq, sysid, compid, msgid[3], payload, checksum, optional signature
```

The checksum input starts at `len` and continues through the actual transmitted payload.

---

## Notes

- The `CRC_EXTRA` value must match the message type and dialect.
- MAVLink v2 can truncate trailing zero bytes from the payload. Therefore, the `len` field may be smaller than the full message payload size defined in the XML specification.
- This project verifies packet integrity only. It does not decode all MAVLink messages.
- For production autopilot communication, use the official MAVLink libraries unless memory or system constraints require a custom implementation.

---

## Repository Description

Use this for the GitHub About section:

```text
Lightweight Python, C, and Arduino implementation for manually calculating and verifying MAVLink packet checksums without the MAVLink library.
```

Suggested topics:

```text
mavlink
crc
checksum
x25-crc
crc16
arduino
embedded-systems
python
c-language
drones
uav
```
