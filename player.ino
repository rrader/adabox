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

// Play a file by full path (command 0x08, auto-plays).
// DY-SV5W requires a special encoding: every '/' after the first becomes "*/",
// '.' becomes '*', and letters are uppercased. So "/MUSIC/00001.mp3" is sent
// on the wire as "/MUSIC*/00001*MP3". Without this conversion the module
// silently ignores the path.
void playerPlayPath(const char *path) {
  uint8_t inLen = strlen(path);
  if (inLen < 1) return;

  uint8_t outLen = inLen;
  for (uint8_t i = 1; i < inLen; i++)
    if (path[i] == '/') outLen++;

  uint8_t cmdLen = 1 + outLen; // device byte + converted path
  uint8_t crc = 0xAA + 0x08 + cmdLen + 0x01;
  Serial2.write(0xAA);
  Serial2.write(0x08);
  Serial2.write(cmdLen);
  Serial2.write(0x01); // SD card

  uint8_t first = (uint8_t)path[0];
  Serial2.write(first);
  crc += first;

  for (uint8_t i = 1; i < inLen; i++) {
    char c = path[i];
    if (c == '/') {
      Serial2.write('*'); crc += '*';
      Serial2.write('/'); crc += '/';
    } else if (c == '.') {
      Serial2.write('*'); crc += '*';
    } else {
      uint8_t up = (uint8_t)toupper((unsigned char)c);
      Serial2.write(up);
      crc += up;
    }
  }
  Serial2.write(crc);
}

// Build path from folder + track number and play.
void playerPlayFolderTrack(const char *folder, int track) {
  char path[32];
  snprintf(path, sizeof(path), "/%s/%05d.mp3", folder, track);
  playerPlayPath(path);
}

// Navigate to a folder and return its track count via command 0x12.
// Called only from setup() — uses delay() which is fine there.
//
// The response (AA 12 02 HI LO SM) can be preceded by stray bytes from the
// preceding playback/stop, so we scan the stream for the header instead of
// assuming a clean 6-byte aligned read.
uint16_t playerQueryFolderCount(const char *folder) {
  playerPlayFolderTrack(folder, 1);
  delay(400); // let module process path and set folder context
  playerStop();
  delay(200);
  while (Serial2.available()) Serial2.read(); // flush

  playerCmd(0x12, 0, nullptr);

  uint32_t deadline = millis() + 700;
  uint8_t state = 0; // 0:want AA, 1:want 12, 2:want 02, 3:HI, 4:LO
  uint8_t hi = 0, lo = 0;
  Serial.print("[Query ");
  Serial.print(folder);
  Serial.print("] rx:");
  while (millis() < deadline) {
    if (!Serial2.available()) continue;
    uint8_t b = Serial2.read();
    Serial.print(' ');
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
    switch (state) {
      case 0: if (b == 0xAA) state = 1; break;
      case 1: state = (b == 0x12) ? 2 : (b == 0xAA ? 1 : 0); break;
      case 2: state = (b == 0x02) ? 3 : (b == 0xAA ? 1 : 0); break;
      case 3: hi = b; state = 4; break;
      case 4: lo = b;
        Serial.println();
        return ((uint16_t)hi << 8) | lo;
    }
  }
  Serial.println(" (timeout)");
  return 0;
}

void playerSeek(int seconds) {
  uint16_t s = (uint16_t)abs(seconds);
  uint8_t d[2] = {(uint8_t)(s >> 8), (uint8_t)(s & 0xFF)};
  playerCmd(seconds >= 0 ? 0x23 : 0x22, 2, d); // 0x23 = FF, 0x22 = RW
}
