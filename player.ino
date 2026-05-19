// ── DY-SV5W UART ─────────────────────────────────────────────────────────────

void playerCmd(uint8_t type, uint8_t len, uint8_t *data) {
  uint8_t crc = 0xAA + type + len;
  Serial2.write(0xAA);
  Serial2.write(type);
  Serial2.write(len);
  for (uint8_t i = 0; i < len; i++) {
    Serial2.write(data[i]);
    crc += data[i];
  }
  Serial2.write(crc);
}

void playerPlay() { playerCmd(0x02, 0, nullptr); }
void playerPause() { playerCmd(0x03, 0, nullptr); }
void playerStop() { playerCmd(0x04, 0, nullptr); }
void playerPrev() { playerCmd(0x05, 0, nullptr); }
void playerNext() { playerCmd(0x06, 0, nullptr); }

void playerVolume(uint8_t vol) { playerCmd(0x13, 1, &vol); }

uint16_t playerQueryTrackCount() {
  while (Serial2.available()) Serial2.read(); // flush
  playerCmd(0x0C, 0, nullptr);
  // response: AA 0C 02 HI LO CRC (6 bytes)
  uint32_t deadline = millis() + 500;
  while (Serial2.available() < 6 && millis() < deadline)
    ;
  if (Serial2.available() < 6) return 0;
  uint8_t buf[6];
  for (int i = 0; i < 6; i++) buf[i] = Serial2.read();
  if (buf[0] == 0xAA && buf[1] == 0x0C && buf[2] == 0x02)
    return ((uint16_t)buf[3] << 8) | buf[4];
  return 0;
}

void playerSelectAndPlay(int track) {
  uint8_t d[2] = {(uint8_t)(track >> 8), (uint8_t)(track & 0xFF)};
  playerCmd(0x07, 2, d); // select + auto-play
}

void playerSeek(int seconds) {
  uint16_t s = (uint16_t)abs(seconds);
  uint8_t d[2] = {(uint8_t)(s >> 8), (uint8_t)(s & 0xFF)};
  playerCmd(seconds >= 0 ? 0x23 : 0x22, 2, d); // 0x23 = FF, 0x22 = RW
}
