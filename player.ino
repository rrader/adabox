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

// Play /FOLDER/00001.mp3 etc. using path-based command 0x08.
void playerPlayPath(const char *folder, int track) {
  char path[32];
  snprintf(path, sizeof(path), "/%s/%05d.mp3", folder, track);
  uint8_t pathLen = strlen(path);
  uint8_t cmdLen = 1 + pathLen; // device byte + path
  uint8_t crc = 0xAA + 0x08 + cmdLen + 0x01;
  Serial2.write(0xAA);
  Serial2.write(0x08);
  Serial2.write(cmdLen);
  Serial2.write(0x01); // SD card
  for (uint8_t i = 0; i < pathLen; i++) {
    Serial2.write((uint8_t)path[i]);
    crc += (uint8_t)path[i];
  }
  Serial2.write(crc);
}

// Navigate to a folder and return its track count via command 0x12.
// Called only from setup() — uses delay() which is fine there.
uint16_t playerQueryFolderCount(const char *folder) {
  playerPlayPath(folder, 1);
  delay(300); // let module process path and set folder context
  playerStop();
  delay(100);
  while (Serial2.available()) Serial2.read(); // flush
  playerCmd(0x12, 0, nullptr);
  // response: AA 12 02 HI LO CRC (6 bytes)
  uint32_t deadline = millis() + 500;
  while (Serial2.available() < 6 && millis() < deadline)
    ;
  if (Serial2.available() < 6) return 0;
  uint8_t buf[6];
  for (int i = 0; i < 6; i++) buf[i] = Serial2.read();
  if (buf[0] == 0xAA && buf[1] == 0x12 && buf[2] == 0x02)
    return ((uint16_t)buf[3] << 8) | buf[4];
  return 0;
}

void playerSeek(int seconds) {
  uint16_t s = (uint16_t)abs(seconds);
  uint8_t d[2] = {(uint8_t)(s >> 8), (uint8_t)(s & 0xFF)};
  playerCmd(seconds >= 0 ? 0x23 : 0x22, 2, d); // 0x23 = FF, 0x22 = RW
}
