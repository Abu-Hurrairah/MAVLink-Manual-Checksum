/*
 * Manual MAVLink checksum verification for Arduino-style boards.
 *
 * This sketch does not use the MAVLink library. It verifies a sample MAVLink v2
 * packet using the X.25 / CRC-16-MCRF4XX checksum and CRC_EXTRA.
 */

#include <Arduino.h>

#define MAVLINK_V1_MAGIC 0xFE
#define MAVLINK_V2_MAGIC 0xFD
#define MAVLINK_V2_IFLAG_SIGNED 0x01
#define X25_INIT_CRC 0xFFFF

uint16_t x25_crc_accumulate(uint8_t byte, uint16_t crc) {
  uint8_t tmp = byte ^ (uint8_t)(crc & 0xFF);
  tmp ^= (uint8_t)(tmp << 4);
  return (uint16_t)((crc >> 8) ^ ((uint16_t)tmp << 8) ^ ((uint16_t)tmp << 3) ^ ((uint16_t)tmp >> 4));
}

uint16_t x25_crc_calculate(const uint8_t *data, uint16_t length, uint8_t crc_extra) {
  uint16_t crc = X25_INIT_CRC;

  for (uint16_t i = 0; i < length; i++) {
    crc = x25_crc_accumulate(data[i], crc);
  }

  crc = x25_crc_accumulate(crc_extra, crc);
  return crc;
}

bool verify_mavlink_frame(const uint8_t *frame, uint16_t frame_length, uint8_t crc_extra, uint16_t *calculated_crc, uint16_t *provided_crc) {
  if (frame_length < 8) {
    return false;
  }

  uint8_t magic = frame[0];
  uint8_t payload_length = frame[1];
  uint16_t checksum_offset = 0;
  uint16_t expected_length = 0;

  if (magic == MAVLINK_V1_MAGIC) {
    checksum_offset = 6 + payload_length;
    expected_length = checksum_offset + 2;
  } else if (magic == MAVLINK_V2_MAGIC) {
    bool signed_packet = (frame[2] & MAVLINK_V2_IFLAG_SIGNED) != 0;
    checksum_offset = 10 + payload_length;
    expected_length = checksum_offset + 2 + (signed_packet ? 13 : 0);
  } else {
    return false;
  }

  if (frame_length != expected_length) {
    return false;
  }

  // Checksum input excludes magic/start byte and checksum bytes.
  *calculated_crc = x25_crc_calculate(&frame[1], checksum_offset - 1, crc_extra);
  *provided_crc = (uint16_t)frame[checksum_offset] | ((uint16_t)frame[checksum_offset + 1] << 8);

  return *calculated_crc == *provided_crc;
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;
  }

  // Example MAVLink v2 frame from the documentation.
  // Message ID: 74 (VFR_HUD), CRC_EXTRA: 20.
  const uint8_t frame[] = {
    0xFD, 0x13, 0x00, 0x00, 0xC3, 0x01, 0x01, 0x4A, 0x00, 0x00,
    0x2E, 0x02, 0x9A, 0x41, 0x84, 0xA0, 0xF6, 0x41, 0xF6, 0xDE,
    0x8E, 0x45, 0x49, 0xA1, 0xBC, 0xBF, 0x2A, 0x01, 0x32,
    0x9B, 0x87
  };

  const uint8_t crc_extra = 20;
  uint16_t calculated_crc = 0;
  uint16_t provided_crc = 0;

  bool valid = verify_mavlink_frame(frame, sizeof(frame), crc_extra, &calculated_crc, &provided_crc);

  Serial.print("Calculated CRC: 0x");
  Serial.println(calculated_crc, HEX);
  Serial.print("Provided CRC:   0x");
  Serial.println(provided_crc, HEX);
  Serial.print("Valid:          ");
  Serial.println(valid ? "true" : "false");
}

void loop() {
  // Nothing to do. This sketch verifies the example frame once in setup().
}
