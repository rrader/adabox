// ── RFID RC522 ───────────────────────────────────────────────────────────────
// SDA(SS) → GPIO 5   SCK → GPIO 25
// MOSI    → GPIO 27  MISO → GPIO 26
// RST     → GPIO 22
//
// Card layout: sector 0, blocks 1+2 (32 bytes) = full file path, null-terminated.
// e.g. "/MUSIC/00003.mp3"
// After each read/write the card is halted — re-present to trigger again.

#define PIN_RFID_SS   5
#define PIN_RFID_RST  22
#define PIN_RFID_SCK  25
#define PIN_RFID_MISO 26
#define PIN_RFID_MOSI 27

MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);

void rfidSetup() {
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  mfrc522.PCD_Init();
  Serial.println("[RFID] RC522 ready");
}

// Authenticate sector 0 (covers blocks 1 and 2).
static bool rfidAuth() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  return mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &mfrc522.uid
  ) == MFRC522::STATUS_OK;
}

// Returns 1 if a valid path was read into outPath,
//        -1 if card is present but blank,
//         0 if no card.
int rfidReadPath(char *outPath, uint8_t maxLen) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return 0;

  int result = -1;
  if (rfidAuth()) {
    char tmp[33] = {};
    byte buf[18];
    byte len;

    len = sizeof(buf);
    if (mfrc522.MIFARE_Read(1, buf, &len) == MFRC522::STATUS_OK)
      memcpy(tmp, buf, 16);

    len = sizeof(buf);
    if (mfrc522.MIFARE_Read(2, buf, &len) == MFRC522::STATUS_OK)
      memcpy(tmp + 16, buf, 16);

    if (tmp[0] == '/') { // valid path starts with /
      strncpy(outPath, tmp, maxLen - 1);
      outPath[maxLen - 1] = '\0';
      result = 1;
    }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return result;
}

// Writes a full path to the card. Returns true on success.
bool rfidWritePath(const char *path) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return false;

  bool ok = false;
  if (rfidAuth()) {
    byte data1[16] = {};
    byte data2[16] = {};
    uint8_t pathLen = strlen(path);
    memcpy(data1, path, min((int)pathLen, 16));
    if (pathLen > 16) memcpy(data2, path + 16, min((int)(pathLen - 16), 16));

    ok = mfrc522.MIFARE_Write(1, data1, 16) == MFRC522::STATUS_OK &&
         mfrc522.MIFARE_Write(2, data2, 16) == MFRC522::STATUS_OK;
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return ok;
}
