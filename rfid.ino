// ── RFID RC522 ───────────────────────────────────────────────────────────────
// SDA(SS) → GPIO 5   SCK → GPIO 25
// MOSI    → GPIO 27  MISO → GPIO 26
// RST     → GPIO 22
//
// Card layout (32 bytes total, null-terminated full file path, e.g. "/MUSIC/00003.mp3"):
//   MIFARE Classic 1K   → sector 0, blocks 1+2  (Crypto1 auth, default KEY_A = FFFFFFFFFFFF)
//   NTAG21x / Ultralight → user pages 4..11      (no auth in factory state)
// Protocol is chosen automatically from SAK. Card is halted after each op —
// re-present to trigger again.

#define PIN_RFID_SS   5
#define PIN_RFID_RST  22
#define PIN_RFID_SCK  25
#define PIN_RFID_MISO 26
#define PIN_RFID_MOSI 27

#define RFID_PATH_BYTES 32

MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);

#define RFID_UNKNOWN 0
#define RFID_CLASSIC 1
#define RFID_NTAG    2

void rfidSetup() {
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  mfrc522.PCD_Init();
  Serial.println("[RFID] RC522 ready");
}

static byte rfidDetectType() {
  switch (mfrc522.PICC_GetType(mfrc522.uid.sak)) {
    case MFRC522::PICC_TYPE_MIFARE_MINI:
    case MFRC522::PICC_TYPE_MIFARE_1K:
    case MFRC522::PICC_TYPE_MIFARE_4K:
      return RFID_CLASSIC;
    case MFRC522::PICC_TYPE_MIFARE_UL:
      return RFID_NTAG;
    default:
      return RFID_UNKNOWN;
  }
}

// ── MIFARE Classic ────────────────────────────────────────────────────────────
static bool classicAuth() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  return mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &mfrc522.uid
  ) == MFRC522::STATUS_OK;
}

static bool classicReadPath(char *out) {
  if (!classicAuth()) return false;
  byte buf[18];
  byte len;

  len = sizeof(buf);
  if (mfrc522.MIFARE_Read(1, buf, &len) != MFRC522::STATUS_OK) return false;
  memcpy(out, buf, 16);

  len = sizeof(buf);
  if (mfrc522.MIFARE_Read(2, buf, &len) != MFRC522::STATUS_OK) return false;
  memcpy(out + 16, buf, 16);
  return true;
}

static bool classicWritePath(byte *data) {
  if (!classicAuth()) return false;
  return mfrc522.MIFARE_Write(1, data,       16) == MFRC522::STATUS_OK &&
         mfrc522.MIFARE_Write(2, data + 16,  16) == MFRC522::STATUS_OK;
}

// ── NTAG21x / Ultralight ──────────────────────────────────────────────────────
// READ (0x30) returns 16 bytes (4 pages) at a time. User memory starts at page 4.
static bool ntagReadPath(char *out) {
  byte buf[18];
  byte len;

  len = sizeof(buf);
  if (mfrc522.MIFARE_Read(4, buf, &len) != MFRC522::STATUS_OK) return false;
  memcpy(out, buf, 16);

  len = sizeof(buf);
  if (mfrc522.MIFARE_Read(8, buf, &len) != MFRC522::STATUS_OK) return false;
  memcpy(out + 16, buf, 16);
  return true;
}

// WRITE (0xA2) takes one 4-byte page at a time → 8 pages cover 32 bytes.
static bool ntagWritePath(byte *data) {
  for (byte i = 0; i < 8; i++) {
    if (mfrc522.MIFARE_Ultralight_Write(4 + i, data + i * 4, 4) != MFRC522::STATUS_OK)
      return false;
  }
  return true;
}

// ── Public API ────────────────────────────────────────────────────────────────
// Returns 1 if a valid path was read into outPath,
//        -1 if card is present but blank/unreadable,
//         0 if no card.
int rfidReadPath(char *outPath, uint8_t maxLen) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return 0;

  char tmp[RFID_PATH_BYTES + 1] = {};
  bool ok = false;
  switch (rfidDetectType()) {
    case RFID_CLASSIC: ok = classicReadPath(tmp); break;
    case RFID_NTAG:    ok = ntagReadPath(tmp);    break;
    default: break;
  }

  int result = -1;
  if (ok && tmp[0] == '/') {
    strncpy(outPath, tmp, maxLen - 1);
    outPath[maxLen - 1] = '\0';
    result = 1;
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return result;
}

// Writes a full path to the card. Returns true on success.
bool rfidWritePath(const char *path) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return false;

  byte data[RFID_PATH_BYTES] = {};
  uint8_t pathLen = strlen(path);
  memcpy(data, path, min((int)pathLen, RFID_PATH_BYTES));

  bool ok = false;
  switch (rfidDetectType()) {
    case RFID_CLASSIC: ok = classicWritePath(data); break;
    case RFID_NTAG:    ok = ntagWritePath(data);    break;
    default: break;
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return ok;
}
