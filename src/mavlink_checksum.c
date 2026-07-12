/*
 * Manual MAVLink checksum calculator and verifier.
 *
 * Supports MAVLink v1 (0xFE) and MAVLink v2 (0xFD) frames without linking the
 * official MAVLink library. Designed to be easy to port to embedded C/Arduino.
 *
 * Compile:
 *     gcc -Wall -Wextra -std=c99 mavlink_checksum.c -o mavlink_checksum
 *
 * Run:
 *     ./mavlink_checksum FD130000C301014A00002E029A4184A0F641F6DE8E4549A1BCBF2A01329B87 20
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAVLINK_V1_MAGIC 0xFE
#define MAVLINK_V2_MAGIC 0xFD
#define MAVLINK_V2_IFLAG_SIGNED 0x01
#define X25_INIT_CRC 0xFFFF
#define MAX_FRAME_BYTES 300

typedef struct {
    uint8_t version;
    uint8_t payload_length;
    uint32_t message_id;
    uint8_t crc_extra;
    uint16_t calculated_crc;
    uint16_t provided_crc;
    uint8_t valid;
} MavlinkChecksumResult;

static uint16_t x25_crc_accumulate(uint8_t byte, uint16_t crc) {
    uint8_t tmp = byte ^ (uint8_t)(crc & 0xFF);
    tmp ^= (uint8_t)(tmp << 4);
    return (uint16_t)((crc >> 8) ^ ((uint16_t)tmp << 8) ^ ((uint16_t)tmp << 3) ^ ((uint16_t)tmp >> 4));
}

static uint16_t x25_crc_calculate(const uint8_t *data, size_t length, uint8_t crc_extra) {
    uint16_t crc = X25_INIT_CRC;

    for (size_t i = 0; i < length; i++) {
        crc = x25_crc_accumulate(data[i], crc);
    }

    crc = x25_crc_accumulate(crc_extra, crc);
    return crc;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int parse_hex_string(const char *hex, uint8_t *out, size_t out_capacity, size_t *out_length) {
    int high_nibble = -1;
    size_t count = 0;

    for (const char *p = hex; *p != '\0'; p++) {
        if (isspace((unsigned char)*p) || *p == ',' || *p == ':') {
            continue;
        }
        if (*p == 'x' || *p == 'X') {
            continue;  // allows strings like 0xFD 0x13
        }

        int value = hex_value(*p);
        if (value < 0) {
            return -1;
        }

        if (high_nibble < 0) {
            high_nibble = value;
        } else {
            if (count >= out_capacity) {
                return -2;
            }
            out[count++] = (uint8_t)((high_nibble << 4) | value);
            high_nibble = -1;
        }
    }

    if (high_nibble >= 0) {
        return -3;  // odd number of hex characters
    }

    *out_length = count;
    return 0;
}

static int calculate_mavlink_checksum(
    const uint8_t *frame,
    size_t frame_length,
    uint8_t crc_extra,
    MavlinkChecksumResult *result
) {
    if (frame_length < 8) {
        return -1;
    }

    uint8_t magic = frame[0];
    uint8_t payload_length = frame[1];
    size_t checksum_offset = 0;
    size_t expected_length = 0;
    uint32_t message_id = 0;
    uint8_t version = 0;

    if (magic == MAVLINK_V1_MAGIC) {
        version = 1;
        checksum_offset = 6u + payload_length;
        expected_length = checksum_offset + 2u;
        if (frame_length != expected_length) {
            return -2;
        }
        message_id = frame[5];
    } else if (magic == MAVLINK_V2_MAGIC) {
        if (frame_length < 12) {
            return -1;
        }
        version = 2;
        uint8_t signed_packet = (frame[2] & MAVLINK_V2_IFLAG_SIGNED) != 0;
        checksum_offset = 10u + payload_length;
        expected_length = checksum_offset + 2u + (signed_packet ? 13u : 0u);
        if (frame_length != expected_length) {
            return -2;
        }
        message_id = (uint32_t)frame[7] | ((uint32_t)frame[8] << 8) | ((uint32_t)frame[9] << 16);
    } else {
        return -3;
    }

    // MAVLink checksum input excludes the magic/start byte and excludes
    // the checksum bytes and optional MAVLink 2 signature.
    const uint8_t *crc_input = &frame[1];
    size_t crc_input_length = checksum_offset - 1u;

    uint16_t calculated_crc = x25_crc_calculate(crc_input, crc_input_length, crc_extra);
    uint16_t provided_crc = (uint16_t)frame[checksum_offset] | ((uint16_t)frame[checksum_offset + 1] << 8);

    result->version = version;
    result->payload_length = payload_length;
    result->message_id = message_id;
    result->crc_extra = crc_extra;
    result->calculated_crc = calculated_crc;
    result->provided_crc = provided_crc;
    result->valid = (calculated_crc == provided_crc);

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mavlink_frame_hex> <crc_extra>\n", argv[0]);
        fprintf(stderr, "Example: %s FD130000C301014A00002E029A4184A0F641F6DE8E4549A1BCBF2A01329B87 20\n", argv[0]);
        return 1;
    }

    long crc_extra_long = strtol(argv[2], NULL, 10);
    if (crc_extra_long < 0 || crc_extra_long > 255) {
        fprintf(stderr, "crc_extra must be between 0 and 255.\n");
        return 1;
    }

    uint8_t frame[MAX_FRAME_BYTES];
    size_t frame_length = 0;
    int parse_status = parse_hex_string(argv[1], frame, sizeof(frame), &frame_length);
    if (parse_status != 0) {
        fprintf(stderr, "Invalid hex string or frame too large. Error code: %d\n", parse_status);
        return 1;
    }

    MavlinkChecksumResult result;
    int status = calculate_mavlink_checksum(frame, frame_length, (uint8_t)crc_extra_long, &result);
    if (status != 0) {
        fprintf(stderr, "Could not parse MAVLink frame. Error code: %d\n", status);
        return 1;
    }

    printf("MAVLink version : v%u\n", result.version);
    printf("Message ID      : %u\n", result.message_id);
    printf("Payload length  : %u\n", result.payload_length);
    printf("CRC_EXTRA       : %u\n", result.crc_extra);
    printf("Calculated CRC  : 0x%04X\n", result.calculated_crc);
    printf("Checksum bytes  : 0x%02X 0x%02X  (little-endian)\n",
           result.calculated_crc & 0xFF,
           (result.calculated_crc >> 8) & 0xFF);
    printf("Provided CRC    : 0x%04X\n", result.provided_crc);
    printf("Valid           : %s\n", result.valid ? "true" : "false");

    return result.valid ? 0 : 2;
}
