/*
 * Jukebox — ESP32 + DY-SV5W + WS2812 16-LED ring
 *
 * Wiring:
 *   DY-SV5W RXD  → ESP32 GPIO 17 (TX2) black
 *   DY-SV5W TXD  → ESP32 GPIO 16 (RX2) white
 *   DY-SV5W BUSY → ESP32 GPIO 15 gray
 *   WS2812 DATA  → ESP32 GPIO 4
 *   BTN_PLAY     → ESP32 GPIO 12  (other leg → GND)
 *   BTN_NEXT     → ESP32 GPIO 13  (other leg → GND)
 *   BTN_PREV     → ESP32 GPIO 14  (other leg → GND)
 *   ENC CLK      → ESP32 GPIO 18
 *   ENC DT       → ESP32 GPIO 19
 *   RC522 SCK    → ESP32 GPIO 25
 *   RC522 MISO   → ESP32 GPIO 26
 *   RC522 MOSI   → ESP32 GPIO 27
 *   RC522 SDA(SS)→ ESP32 GPIO 5
 *   RC522 RST    → ESP32 GPIO 22
 *
 * LED ring behavior:
 *   Stopped  — slow blue breathing
 *   Playing  — rainbow spinner (speed stays constant)
 *   Next     — clockwise green sweep, then resume
 *   Prev     — counter-clockwise yellow sweep, then resume
 *   Play     — green burst flash
 *   Stop     — red burst flash
 *   Track end — white burst, auto-advances
 *
 * Library deps (install via Arduino Library Manager):
 *   FastLED  ≥ 3.6
 */

struct Btn {
  uint8_t pin;
  bool state;
  bool lastRaw;
  uint32_t lastChange;
};

#include <FastLED.h>
#include <SPI.h>
#include <MFRC522.h>

// ── Pin map ──────────────────────────────────────────────────────────────────
#define PIN_TX 17   // to DY-SV5W RXD
#define PIN_RX 16   // to DY-SV5W TXD
#define PIN_BUSY 15 // DY-SV5W BUSY (LOW = playing)
#define PIN_LED 4   // WS2812 data
#define PIN_BTN_PLAY 12
#define PIN_BTN_NEXT 14
#define PIN_BTN_PREV 13

// ── Constants ────────────────────────────────────────────────────────────────
#define NUM_LEDS 16
#define PLAYER_BAUD 9600
#define MUSIC_FOLDER "MUSIC"
#define RFID_FOLDER  "OTHER"
#define DEBOUNCE_MS 50
#define LED_BRIGHTNESS       90  // MUSIC mode
#define LED_BRIGHTNESS_OTHER 35  // OTHER mode — dim and relaxing
#define VOLUME_MAX     21  // ~70% of the 0-30 scale
#define VOLUME_DEFAULT 15
#define SEEK_SECONDS 5   // seconds per encoder tick in seek mode

// ── State ────────────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];

enum AnimMode { ANIM_BREATHE, ANIM_SPINNER, ANIM_SWEEP_CW, ANIM_SWEEP_CCW, ANIM_FLASH, ANIM_SEEK, ANIM_PROGRAM, ANIM_PROGRAM_SUCCESS };
AnimMode animMode = ANIM_BREATHE;
AnimMode prevMode = ANIM_BREATHE;

enum PlayContext { CTX_MUSIC, CTX_RFID };
enum BrowseMode  { BROWSE_MUSIC, BROWSE_RFID };

bool isPlaying = false;
PlayContext playContext = CTX_MUSIC;
BrowseMode  browseMode  = BROWSE_MUSIC;
int musicTrack = 1;         // current position in MUSIC folder
int totalMusicTracks = 1;   // set at boot
int rfidTrack = 1;          // current position in RFID folder
int totalRfidTracks = 1;    // set at boot
char lastRfidPath[32] = {}; // path of last played RFID card

#define LONG_PRESS_MS 5000
#define PROGRAM_MIN_MS 5000
uint32_t nextPressTime        = 0;
bool     nextLongPressHandled = false;
int currentVolume = VOLUME_DEFAULT;
bool seekUsed = false;
AnimMode preProgramMode = ANIM_BREATHE;
bool nextHeldWithPrev = false;
bool prevHeldWithNext = false;
bool inProgramMode = false;
uint32_t programModeStart = 0;

// FNV-1a hash — used to derive a stable seed from an RFID card's path.
uint32_t hashPath(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

void applyBrightness() {
  FastLED.setBrightness(
    isPlaying && playContext == CTX_RFID ? LED_BRIGHTNESS_OTHER : LED_BRIGHTNESS
  );
}

// ── Button ───────────────────────────────────────────────────────────────────
Btn btnPlay = {PIN_BTN_PLAY, HIGH, HIGH, 0};
Btn btnNext = {PIN_BTN_NEXT, HIGH, HIGH, 0};
Btn btnPrev = {PIN_BTN_PREV, HIGH, HIGH, 0};

// Returns 1 on press (→LOW), -1 on release (→HIGH), 0 otherwise.
int pollButton(Btn &b) {
  bool raw = digitalRead(b.pin);
  uint32_t now = millis();
  if (raw != b.lastRaw)
    b.lastChange = now;
  b.lastRaw = raw;
  if ((now - b.lastChange) > DEBOUNCE_MS && raw != b.state) {
    b.state = raw;
    return (b.state == LOW) ? 1 : -1;
  }
  return 0;
}

// ── Busy-pin track-end detection
// ──────────────────────────────────────────────
uint32_t busyHighSince = 0;
bool prevBusy = false; // true = was-playing (BUSY LOW)

void checkTrackEnd() {
  bool busyNow = (digitalRead(PIN_BUSY) == LOW); // LOW = playing
  uint32_t now = millis();

  if (busyNow) {
    busyHighSince = now;
    prevBusy = true;
  } else if (prevBusy && (now - busyHighSince > 200)) {
    prevBusy = false;
    isPlaying = false;
    startFlash(CRGB::White);
    if (playContext == CTX_MUSIC) {
      musicTrack = (musicTrack % totalMusicTracks) + 1;
      Serial.print("[BUSY] Track ended → auto-advance to MUSIC track ");
      Serial.println(musicTrack);
      playerPlayFolderTrack(MUSIC_FOLDER, musicTrack);
      isPlaying = true;
      applyBrightness();
      randomizeSpinner(musicTrack);
      animMode = ANIM_SPINNER;
    } else {
      applyBrightness(); // restore brightness after RFID track ends
    }
  }
}

// ── Setup / Loop
// ──────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(PLAYER_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  pinMode(PIN_BTN_PLAY, INPUT_PULLUP);
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
  pinMode(PIN_BTN_PREV, INPUT_PULLUP);
  pinMode(PIN_BUSY, INPUT);

  ledSetup();
  encoderSetup();
  rfidSetup();

  delay(800); // let DY-SV5W boot
  playerVolume(currentVolume);

  totalMusicTracks = playerQueryFolderCount(MUSIC_FOLDER);
  if (totalMusicTracks == 0) totalMusicTracks = 1;
  Serial.print("[Jukebox] MUSIC tracks: ");
  Serial.println(totalMusicTracks);

  totalRfidTracks = playerQueryFolderCount(RFID_FOLDER);
  if (totalRfidTracks == 0) totalRfidTracks = 1;
  Serial.print("[Jukebox] RFID tracks: ");
  Serial.println(totalRfidTracks);

  Serial.println("[Jukebox] Ready.");
}

void loop() {
  // ── Encoder ──
  int delta = drainEncoder();
  if (delta != 0) {
    if (btnPlay.state == LOW) {
      int secs = delta * SEEK_SECONDS;
      Serial.print("[ENC] Seek ");
      Serial.print(secs > 0 ? "+" : "");
      Serial.print(secs);
      Serial.println("s");
      playerSeek(secs);
      stepSeekSweep(delta > 0);
      seekUsed = true;
    } else {
      currentVolume = constrain(currentVolume + delta, 0, VOLUME_MAX);
      Serial.print("[ENC] Volume → ");
      Serial.println(currentVolume);
      playerVolume(currentVolume);
    }
  }

  // ── Play / Stop ──
  int playEvent = pollButton(btnPlay);
  if (playEvent == 1) {
    seekUsed = false;
  }

  if (playEvent == -1 && seekUsed) {
    animMode = prevMode;
  }
  if (playEvent == -1 && !seekUsed) {
    if (isPlaying) {
      Serial.println("[BTN] PLAY → Stop");
      playerStop();
      isPlaying = false;
      prevBusy = false;
      animMode = ANIM_BREATHE;
      applyBrightness();
      startFlash(CRGB::Red);
    } else {
      uint32_t spinSeed;
      if (browseMode == BROWSE_MUSIC) {
        Serial.print("[BTN] PLAY → MUSIC track ");
        Serial.println(musicTrack);
        playerPlayFolderTrack(MUSIC_FOLDER, musicTrack);
        playContext = CTX_MUSIC;
        spinSeed = musicTrack;
      } else {
        Serial.print("[BTN] PLAY → RFID track ");
        Serial.println(rfidTrack);
        playerPlayFolderTrack(RFID_FOLDER, rfidTrack);
        playContext = CTX_RFID;
        spinSeed = rfidTrack;
      }
      isPlaying = true;
      applyBrightness();
      randomizeSpinner(spinSeed);
      animMode = ANIM_SPINNER;
      startFlash(CRGB::Green);
    }
  }

  // ── Next ──
  int nextEvent = pollButton(btnNext);
  if (nextEvent == 1) {
    nextHeldWithPrev = (btnPrev.state == LOW) || inProgramMode;
    nextPressTime = millis();
    nextLongPressHandled = false;
  }

  // Long press while stopped: toggle browse mode (suppresses the eventual release).
  if (btnNext.state == LOW && !nextLongPressHandled && !nextHeldWithPrev &&
      !isPlaying && millis() - nextPressTime >= LONG_PRESS_MS) {
    nextLongPressHandled = true;
    browseMode = (browseMode == BROWSE_MUSIC) ? BROWSE_RFID : BROWSE_MUSIC;
    Serial.println(browseMode == BROWSE_MUSIC ? "[MODE] MUSIC" : "[MODE] OTHER");
    startFlash(browseMode == BROWSE_MUSIC ? CRGB::Orange : CRGB::Purple);
  }

  if (nextEvent == -1 && !nextHeldWithPrev && !nextLongPressHandled) {
    uint32_t spinSeed;
    if (browseMode == BROWSE_MUSIC) {
      musicTrack = (musicTrack % totalMusicTracks) + 1;
      Serial.print("[BTN] NEXT → MUSIC track "); Serial.println(musicTrack);
      playerPlayFolderTrack(MUSIC_FOLDER, musicTrack);
      playContext = CTX_MUSIC;
      spinSeed = musicTrack;
    } else {
      rfidTrack = (rfidTrack % totalRfidTracks) + 1;
      Serial.print("[BTN] NEXT → RFID track "); Serial.println(rfidTrack);
      playerPlayFolderTrack(RFID_FOLDER, rfidTrack);
      playContext = CTX_RFID;
      spinSeed = rfidTrack;
    }
    isPlaying = true;
    applyBrightness();
    randomizeSpinner(spinSeed);
    startSweep(true, CRGB::Cyan);
    prevMode = ANIM_SPINNER;
  }

  // ── Prev ──
  int prevEvent = pollButton(btnPrev);
  if (prevEvent == 1)  prevHeldWithNext = (btnNext.state == LOW) || inProgramMode;
  if (prevEvent == -1 && !prevHeldWithNext) {
    uint32_t spinSeed;
    if (browseMode == BROWSE_MUSIC) {
      musicTrack = (musicTrack - 2 + totalMusicTracks) % totalMusicTracks + 1;
      Serial.print("[BTN] PREV → MUSIC track "); Serial.println(musicTrack);
      playerPlayFolderTrack(MUSIC_FOLDER, musicTrack);
      playContext = CTX_MUSIC;
      spinSeed = musicTrack;
    } else {
      rfidTrack = (rfidTrack - 2 + totalRfidTracks) % totalRfidTracks + 1;
      Serial.print("[BTN] PREV → RFID track "); Serial.println(rfidTrack);
      playerPlayFolderTrack(RFID_FOLDER, rfidTrack);
      playContext = CTX_RFID;
      spinSeed = rfidTrack;
    }
    isPlaying = true;
    applyBrightness();
    randomizeSpinner(spinSeed);
    startSweep(false, CRGB::Yellow);
    prevMode = ANIM_SPINNER;
  }

  // ── RFID programming mode ──
  // Both NEXT+PREV held → enter programming mode. Stays active for at least
  // PROGRAM_MIN_MS after release; exits early on a successful write.
  bool comboNow = (btnNext.state == LOW && btnPrev.state == LOW);
  if (comboNow) {
    nextHeldWithPrev = true;
    prevHeldWithNext = true;
    if (!inProgramMode) {
      inProgramMode = true;
      programModeStart = millis();
      preProgramMode = animMode;
      animMode = ANIM_PROGRAM;
    }
  }

  // ── RFID ──
  if (inProgramMode) {
    char path[32];
    if (browseMode == BROWSE_MUSIC)
      snprintf(path, sizeof(path), "/%s/%05d.mp3", MUSIC_FOLDER, musicTrack);
    else
      snprintf(path, sizeof(path), "/%s/%05d.mp3", RFID_FOLDER, rfidTrack);
    bool wrote = rfidWritePath(path);
    if (wrote) {
      Serial.print("[RFID] Wrote path ");
      Serial.println(path);
      inProgramMode = false;
      prevMode = preProgramMode; // success anim returns to where we came from
      startProgramSuccess();
    } else if (!comboNow && (millis() - programModeStart) >= PROGRAM_MIN_MS) {
      inProgramMode = false;
      animMode = preProgramMode;
    }
  } else {
    char rfidPath[32];
    int result = rfidReadPath(rfidPath, sizeof(rfidPath));
    if (result == 1) {
      if (isPlaying && playContext == CTX_RFID && strcmp(rfidPath, lastRfidPath) == 0) {
        Serial.println("[RFID] Already playing this track, ignoring");
      } else {
        strncpy(lastRfidPath, rfidPath, sizeof(lastRfidPath));
        playContext = CTX_RFID;
        Serial.print("[RFID] Playing ");
        Serial.println(rfidPath);
        playerPlayPath(rfidPath);
        isPlaying = true;
        applyBrightness();
        randomizeSpinner(hashPath(rfidPath));
        animMode = ANIM_SPINNER;
        startFlash(CRGB::Magenta);
      }
    } else if (result == -1) {
      Serial.println("[RFID] Card has no path written");
      startFlash(CRGB::Red);
    }
  }

  if (isPlaying)
    checkTrackEnd();

  updateLEDs();
}
