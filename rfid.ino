// ── RFID RC522 ───────────────────────────────────────────────────────────────
// SDA(SS) → GPIO 5   SCK → GPIO 25
// MOSI    → GPIO 27  MISO → GPIO 26
// RST     → GPIO 22
//
// Card layout: block 1, bytes 0-1 = track number (big-endian uint16).
// After each read/write the card is halted — it must be removed and
// re-presented to trigger again.

#define PIN_RFID_SS   5
#define PIN_RFID_RST  22
#define PIN_RFID_SCK  25
#define PIN_RFID_MISO 26
#define PIN_RFID_MOSI 27
#define RFID_BLOCK    1  // block inside sector 0 used for track storage

MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);

void rfidSetup() {
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  mfrc522.PCD_Init();
  Serial.println("[RFID] RC522 ready");
}

static bool rfidAuth(byte block) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF; // factory default
  return mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &mfrc522.uid
  ) == MFRC522::STATUS_OK;
}

// Returns track number (≥1) if a programmed card is present,
// -1 if a card is present but has no track written, 0 if no card.
int rfidRead() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return 0;

  int track = -1; // card present, assume blank until proven otherwise
  if (rfidAuth(RFID_BLOCK)) {
    byte buf[18];
    byte len = sizeof(buf);
    if (mfrc522.MIFARE_Read(RFID_BLOCK, buf, &len) == MFRC522::STATUS_OK) {
      uint16_t t = ((uint16_t)buf[0] << 8) | buf[1];
      if (t >= 1) track = t;
    }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return track;
}

// Writes track number to card. Returns true on success.
bool rfidWrite(int track) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return false;

  bool ok = false;
  if (rfidAuth(RFID_BLOCK)) {
    byte data[16] = {};
    data[0] = (uint8_t)(track >> 8);
    data[1] = (uint8_t)(track & 0xFF);
    ok = mfrc522.MIFARE_Write(RFID_BLOCK, data, 16) == MFRC522::STATUS_OK;
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return ok;
}
