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
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>

#define FW_VERSION  "2.3.3"
#define DEVICE_NAME "adabox"
#define AP_SSID     "AdaBox-Setup"

void stepBootAnimation();
void delayWithBootAnimation(uint32_t ms);
void startSweep(bool cw, CRGB color);


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
#define LED_BRIGHTNESS       90  // MUSIC mode, at max level
#define LED_BRIGHTNESS_OTHER 35  // OTHER mode — dim and relaxing, at max level
#define LED_LEVEL_MIN     0      // user brightness level: base * level / LED_LEVEL_MAX (0 = LEDs off)
#define LED_LEVEL_MAX     10     // (10 → 1.0x = the base values above, 1 → 0.1x, 0 → off)
#define LED_LEVEL_DEFAULT LED_LEVEL_MAX
#define VOLUME_MAX     14  // 21 / 1.5 (~47% of the 0-30 scale)
#define VOLUME_MIN     2
#define VOLUME_DEFAULT 8
#define IDLE_SLEEP_MS (10UL * 60UL * 1000UL) // deep sleep after 10 min of inactivity
#define FACTORY_RESET_MS 15000   // all 3 buttons held this long → wipe saved state
#define OVL_VOLUME 1             // LED overlays (see leds.ino): volume arc,
#define OVL_LED    2             // LED brightness arc,
#define OVL_TRACK  3             // track number (dots + colour per ring of 16)
#define OVERLAY_VALUE_MS 1200
#define OVERLAY_TRACK_MS 2000
#define SEEK_SECONDS 5   // seconds per encoder tick in seek mode

// ── State ────────────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];

Preferences prefs;
int savedVolume = -1;
uint32_t volumeChangedAt = 0;
#define VOLUME_SAVE_DELAY_MS 2000

int ledLevel = LED_LEVEL_DEFAULT;
int savedLedLevel = -1;
uint32_t ledLevelChangedAt = 0;

int savedMusicTrack = -1, savedRfidTrack = -1, savedBrowse = -1;

WebServer server(80);
DNSServer dnsServer;
volatile bool isOtaUpdating = false;
bool apMode = false;
bool wifiConnected = false;

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

// ── Wi-Fi Captive Portal & WebServer ──────────────────────────────────────────
void setupWebServer() {
  server.on("/", []() {
    if (apMode) {
      // Hotspot Wi-Fi Setup Form
      String s = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
      s += "<title>AdaBox Setup</title><style>";
      s += "body{font-family:system-ui,-apple-system,sans-serif;background:#12121e;color:#fff;display:flex;justify-content:center;padding:20px;margin:0}";
      s += ".card{background:#1e1e2f;padding:24px;border-radius:14px;max-width:340px;width:100%;box-shadow:0 8px 24px rgba(0,0,0,0.5)}";
      s += "h2{color:#e0457a;margin:0 0 8px 0;font-size:20px}p{color:#aaa;font-size:13px;margin:0 0 16px 0}";
      s += "label{display:block;font-size:12px;color:#888;margin-bottom:6px}";
      s += "input,select{width:100%;padding:10px;margin-bottom:14px;background:#2b2b3d;border:1px solid #444;border-radius:8px;color:#fff;font-size:14px;box-sizing:border-box}";
      s += "button{width:100%;padding:12px;background:#e0457a;border:none;border-radius:8px;color:#fff;font-weight:600;font-size:15px;cursor:pointer}";
      s += "</style></head><body><div class='card'>";
      s += "<h2>📻 AdaBox Setup</h2><p>Connect your music box to home Wi-Fi (v" FW_VERSION "):</p>";
      s += "<form method='POST' action='/save'>";
      s += "<label>Wi-Fi Network (SSID):</label>";
      s += "<select name='ssid'>";
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; ++i) {
        String net = WiFi.SSID(i);
        s += "<option value='" + net + "'>" + net + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
      }
      s += "</select>";
      s += "<label>Or type manually:</label><input type='text' name='manual_ssid' placeholder='Network name'>";
      s += "<label>Wi-Fi Password:</label><input type='password' name='pass' placeholder='Password'>";
      s += "<button type='submit'>Save & Connect</button></form></div></body></html>";
      server.send(200, "text/html", s);
    } else {
      // Normal Dashboard
      String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
      html += "<title>AdaBox</title><style>body{font-family:sans-serif;background:#181824;color:#eee;padding:24px;text-align:center}";
      html += ".card{background:#222234;max-width:320px;margin:0 auto;padding:20px;border-radius:12px;border:1px solid #333}";
      html += "h1{color:#e0457a;font-size:22px;margin:0 0 10px 0}.badge{background:#2d4a22;color:#7ee787;padding:4px 8px;border-radius:6px;font-size:12px}";
      html += "p{margin:10px 0;font-size:14px}.btn{display:inline-block;margin-top:14px;color:#ff8080;text-decoration:none;font-size:12px}</style></head>";
      html += "<body><div class='card'><h1>📻 AdaBox</h1><p><span class='badge'>v" FW_VERSION " Online</span></p>";
      html += "<p>Mode: <b>" + String(browseMode == BROWSE_MUSIC ? "MUSIC" : "OTHER") + "</b></p>";
      html += "<p>Track: <b>#" + String(browseMode == BROWSE_MUSIC ? musicTrack : rfidTrack) + "</b> (" + String(isPlaying ? "Playing ▶" : "Stopped ⏹") + ")</p>";
      html += "<p>Volume: <b>" + String(currentVolume) + "</b> / " + String(VOLUME_MAX) + "</p>";
      html += "<p>LED Level: <b>" + String(ledLevel) + "</b> / 10</p>";
      html += "<p><a href='/update' style='display:inline-block;margin-top:14px;color:#80d4ff;text-decoration:none;font-size:12px;'>⚡ OTA Firmware Update</a></p>";
      html += "<p><a href='/reset-wifi' class='btn' >⚠ Reset Wi-Fi</a></p>";
      html += "</div></body></html>";
      server.send(200, "text/html", html);
    }
  });

  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("manual_ssid");
    if (ssid.length() == 0) ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim(); pass.trim();

    prefs.begin("jukebox", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.end();

    String s = "<!DOCTYPE html><html><body style='background:#12121e;color:#7ee787;font-family:sans-serif;text-align:center;padding:40px;'>";
    s += "<h2>✓ Credentials Saved!</h2><p style='color:#ccc'>Connecting to " + ssid + "... AdaBox is rebooting.</p></body></html>";
    server.send(200, "text/html", s);
    delay(1000);
    ESP.restart();
  });

  server.on("/info", []() {
    String json = "{\"name\":\"adabox\",\"version\":\"" FW_VERSION "\",";
    json += "\"status\":\"" + String(isPlaying ? "playing" : "idle") + "\",";
    json += "\"mode\":\"" + String(browseMode == BROWSE_MUSIC ? "MUSIC" : "OTHER") + "\",";
    json += "\"track\":" + String(browseMode == BROWSE_MUSIC ? musicTrack : rfidTrack) + ",";
    json += "\"volume\":" + String(currentVolume) + ",";
    json += "\"led\":" + String(ledLevel) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    server.send(200, "application/json", json);
  });

  server.on("/reset-wifi", []() {
    prefs.begin("jukebox", false);
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");
    prefs.end();
    server.send(200, "text/html", "Wi-Fi reset. Rebooting to AdaBox-Setup hotspot...");
    delay(1000);
    ESP.restart();
  });

  server.on("/update", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>AdaBox Update</title><style>body{font-family:sans-serif;background:#181824;color:#eee;padding:24px;text-align:center}";
    html += ".card{background:#222234;max-width:340px;margin:0 auto;padding:24px;border-radius:12px;border:1px solid #333}";
    html += "h1{color:#80d4ff;font-size:20px;margin:0 0 12px 0}input[type=file]{margin:16px 0;width:100%;color:#aaa}";
    html += "input[type=submit]{background:#388bfd;color:#fff;border:none;padding:10px 20px;border-radius:8px;font-size:14px;cursor:pointer;width:100%}";
    html += "a{color:#888;text-decoration:none;font-size:12px;display:inline-block;margin-top:16px}</style></head>";
    html += "<body><div class='card'><h1>📻 AdaBox OTA Update</h1>";
    html += "<p style='font-size:13px;color:#aaa'>Select compiled <code>jukebox.ino.bin</code>:</p>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update' accept='.bin' required><br>";
    html += "<input type='submit' value='Flash Firmware'>";
    html += "</form><a href='/'>← Back to Dashboard</a></div></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      isOtaUpdating = true;
      Serial.printf("[WebOTA] Update starting: %s\n", upload.filename.c_str());
      playerStop();
      fill_solid(leds, NUM_LEDS, CRGB::Blue);
      FastLED.show();
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("[WebOTA] Success: %u bytes! Rebooting...\n", upload.totalSize);
        fill_solid(leds, NUM_LEDS, CRGB::Green);
        FastLED.show();
      } else {
        Update.printError(Serial);
        fill_solid(leds, NUM_LEDS, CRGB::Red);
        FastLED.show();
      }
      isOtaUpdating = false;
    }
  });

  server.onNotFound([]() {
    if (apMode) {
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
  Serial.println("[HTTP] Web server started.");
}

void startHotspot() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  delay(100);
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(53, "*", apIP);
  setupWebServer();
  Serial.print("[WiFi] Hotspot started: "); Serial.println(AP_SSID);
  Serial.print("[WiFi] Connect and visit: http://"); Serial.println(apIP);
}

void wifiSetup() {
  prefs.begin("jukebox", true);
  String ssid = prefs.getString("wifi_ssid", "");
  String pass = prefs.getString("wifi_pass", "");
  prefs.end();

  if (ssid.length() == 0) {
    Serial.println("[WiFi] No saved credentials. Starting setup hotspot...");
    startHotspot();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable modem sleep to prevent CPU frequency throttling and WiFi latency jitter
  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("[WiFi] Connecting to "); Serial.print(ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start < 8000)) {
    stepBootAnimation();
    delay(40);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.print("[WiFi] Connected! IP: "); Serial.println(WiFi.localIP());
    setupWebServer();
  } else {
    Serial.println("[WiFi] Connection failed. Starting fallback hotspot...");
    startHotspot();
  }
}

// ── Persistent state (NVS): last track per mode + last browse mode ──────────

void loadState() {
  prefs.begin("jukebox", true);
  musicTrack = prefs.getInt("music", 1);
  rfidTrack  = prefs.getInt("rfid", 1);
  browseMode = prefs.getInt("mode", 0) ? BROWSE_RFID : BROWSE_MUSIC;
  currentVolume = constrain(prefs.getInt("vol", VOLUME_DEFAULT), VOLUME_MIN, VOLUME_MAX);
  ledLevel = constrain(prefs.getInt("led", LED_LEVEL_DEFAULT), LED_LEVEL_MIN, LED_LEVEL_MAX);
  prefs.end();
  savedVolume = currentVolume;
  savedLedLevel = ledLevel;
  if (musicTrack < 1 || musicTrack > totalMusicTracks) musicTrack = 1;
  if (rfidTrack  < 1 || rfidTrack  > totalRfidTracks)  rfidTrack  = 1;
  savedMusicTrack = musicTrack;
  savedRfidTrack = rfidTrack;
  savedBrowse = (browseMode == BROWSE_RFID);
}

// Writes to flash only when something actually changed.
void saveStateIfChanged() {
  int b = (browseMode == BROWSE_RFID);
  bool volDirty = currentVolume != savedVolume &&
                  millis() - volumeChangedAt >= VOLUME_SAVE_DELAY_MS;
  bool ledDirty = ledLevel != savedLedLevel &&
                  millis() - ledLevelChangedAt >= VOLUME_SAVE_DELAY_MS;
  if (musicTrack == savedMusicTrack && rfidTrack == savedRfidTrack && b == savedBrowse && !volDirty && !ledDirty) return;
  prefs.begin("jukebox", false);
  if (ledDirty) { prefs.putInt("led", ledLevel); savedLedLevel = ledLevel; }
  if (volDirty) { prefs.putInt("vol", currentVolume); savedVolume = currentVolume; }
  prefs.putInt("music", musicTrack);
  prefs.putInt("rfid", rfidTrack);
  prefs.putInt("mode", b);
  prefs.end();
  savedMusicTrack = musicTrack; savedRfidTrack = rfidTrack; savedBrowse = b;
}

// ── OTHER-mode resume position ───────────────────────────────────────────────
// The DY-SV5W cannot report the playback time, so we count it ourselves
// (wall-clock while playing, adjusted by seeks) and store it per OTHER track
// in NVS (key "p<N>", seconds). Play button / card tap resumes via seek.
#define POS_SAVE_EVERY_MS 15000
#define PREV_RESTART_S    10    // PREV after this many seconds restarts the track
#define POS_MIN_RESUME_S  5     // below this → start from the beginning
int      otherPlaying   = 0;    // OTHER track number being tracked (0 = none)
int32_t  posBaseMs      = 0;    // position at posStartMs
uint32_t posStartMs     = 0;
uint32_t lastPosSaveMs  = 0;

int currentPosSec() {
  int32_t ms = posBaseMs + (int32_t)(millis() - posStartMs);
  return ms < 0 ? 0 : ms / 1000;
}

void savePos(int track, int sec) {
  char key[8];
  snprintf(key, sizeof(key), "p%d", track);
  prefs.begin("jukebox", false);
  prefs.putInt(key, sec);
  prefs.end();
}

int loadPos(int track) {
  char key[8];
  snprintf(key, sizeof(key), "p%d", track);
  prefs.begin("jukebox", true);
  int v = prefs.getInt(key, 0);
  prefs.end();
  return v;
}

// Call right after the OTHER track started playing from the beginning.
// resume=true → seek to the saved position.
void trackOther(int track, bool resume) {
  int sec = resume ? loadPos(track) : 0;
  if (sec < POS_MIN_RESUME_S) sec = 0;
  if (sec > 0) {
    delay(300);               // let the module start the file first
    playerSeek(sec);
    Serial.print("[POS] Resume OTHER track "); Serial.print(track);
    Serial.print(" at "); Serial.print(sec); Serial.println("s");
  }
  otherPlaying = track;
  posBaseMs = (int32_t)sec * 1000;
  posStartMs = millis();
  lastPosSaveMs = millis();
}

// Stop tracking; keepPos=false clears the saved position (track finished).
void untrackOther(bool keepPos) {
  if (otherPlaying > 0) savePos(otherPlaying, keepPos ? currentPosSec() : 0);
  otherPlaying = 0;
}

// ── Factory reset ────────────────────────────────────────────────────────────
// Erases the whole "jukebox" NVS namespace (tracks, mode, volume, LED level,
// OTHER positions), blinks the ring red regardless of the LED level, reboots.
void factoryReset() {
  Serial.println("[RESET] Erasing saved state");
  playerStop();
  prefs.begin("jukebox", false);
  prefs.clear();
  prefs.end();
  FastLED.setBrightness(255);
  for (int i = 0; i < 6; i++) {
    fill_solid(leds, NUM_LEDS, i % 2 == 0 ? CRGB::Red : CRGB::Black);
    FastLED.show();
    delay(250);
  }
  ESP.restart();
}

// ── Auto power-off (deep sleep) ──────────────────────────────────────────────
// Idle = not playing and no button / encoder / card activity for IDLE_SLEEP_MS.
// Wake-up: Play button (GPIO 12 → LOW). The ESP32 reboots, state is restored
// from NVS. ext0 is the only wake source usable with an active-LOW button
// (ext1 ANY_LOW is unsupported on the original ESP32).
uint32_t lastActivityMs = 0;

void goToSleep() {
  Serial.println("[SLEEP] Idle timeout → deep sleep");
  saveStateIfChanged();
  playerStop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  FastLED.clear(true);
  digitalWrite(PIN_LED, LOW);
  // Wait until Play is released, otherwise we'd wake up immediately.
  while (digitalRead(PIN_BTN_PLAY) == LOW) delay(10);
  rtc_gpio_pullup_en(GPIO_NUM_12);
  rtc_gpio_pulldown_dis(GPIO_NUM_12);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0);
  Serial.flush();
  esp_deep_sleep_start();
}

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
  int base = isPlaying && playContext == CTX_RFID ? LED_BRIGHTNESS_OTHER : LED_BRIGHTNESS;
  FastLED.setBrightness(constrain(base * ledLevel / LED_LEVEL_MAX, 0, 255));
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
      otherPlaying = 0;
      isPlaying = true;
      applyBrightness();
      randomizeSpinner(musicTrack);
      animMode = ANIM_SPINNER;
      showOverlay(OVL_TRACK, musicTrack);
    } else {
      if (otherPlaying > 0) untrackOther(false); // finished → start over next time
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

  loadState();       // Restore volume and ledLevel immediately
  ledSetup();
  applyBrightness();
  stepBootAnimation(); // First visual frame lit instantly within 10ms of power-on!

  encoderSetup();

  delayWithBootAnimation(800); // let DY-SV5W boot while spinner glides smoothly
  playerVolume(currentVolume);

  totalMusicTracks = playerQueryFolderCount(MUSIC_FOLDER);
  if (totalMusicTracks == 0) totalMusicTracks = 1;
  musicTrack = constrain(musicTrack, 1, (int)totalMusicTracks);
  Serial.print("[Jukebox] MUSIC tracks: ");
  Serial.println(totalMusicTracks);

  totalRfidTracks = playerQueryFolderCount(RFID_FOLDER);
  if (totalRfidTracks == 0) totalRfidTracks = 1;
  rfidTrack = constrain(rfidTrack, 1, (int)totalRfidTracks);
  Serial.print("[Jukebox] RFID tracks: ");
  Serial.println(totalRfidTracks);

  playerVolume(currentVolume); // apply restored volume
  applyBrightness();           // apply restored LED brightness
  Serial.print("[Jukebox] Restored: vol=");
  Serial.print(currentVolume);
  Serial.print(" mode=");
  Serial.print(browseMode == BROWSE_MUSIC ? "MUSIC" : "OTHER");
  Serial.print(" music=");
  Serial.print(musicTrack);
  Serial.print(" other=");
  Serial.println(rfidTrack);

  rfidSetup();

  lastActivityMs = millis();
  wifiSetup();
  startSweep(true, CRGB::Green); // Ready sweep!

  Serial.print("[Jukebox] Version v");
  Serial.print(FW_VERSION);
  Serial.println(" Ready.");
}

void loop() {
  if (isOtaUpdating) {
    server.handleClient();
    return;
  }

  // ── Encoder ──
  int delta = drainEncoder();
  if (delta != 0) {
    if (btnPrev.state == LOW) {
      // PREV held + encoder → LED brightness (suppresses the PREV action on release)
      prevHeldWithNext = true;
      ledLevel = constrain(ledLevel + delta, LED_LEVEL_MIN, LED_LEVEL_MAX);
      Serial.print("[ENC] LED level → "); Serial.println(ledLevel);
      ledLevelChangedAt = millis();
      applyBrightness();
      showOverlay(OVL_LED, ledLevel);
    } else if (btnPlay.state == LOW) {
      int secs = delta * SEEK_SECONDS;
      Serial.print("[ENC] Seek ");
      Serial.print(secs > 0 ? "+" : "");
      Serial.print(secs);
      Serial.println("s");
      playerSeek(secs);
      if (otherPlaying > 0) posBaseMs += (int32_t)secs * 1000; // keep our counter in sync
      stepSeekSweep(delta > 0);
      seekUsed = true;
    } else {
      currentVolume = constrain(currentVolume + delta, VOLUME_MIN, VOLUME_MAX);
      Serial.print("[ENC] Volume → ");
      Serial.println(currentVolume);
      playerVolume(currentVolume);
      volumeChangedAt = millis();
      showOverlay(OVL_VOLUME, currentVolume);
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
      untrackOther(true);
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
        otherPlaying = 0;
      } else {
        Serial.print("[BTN] PLAY → RFID track ");
        Serial.println(rfidTrack);
        playerPlayFolderTrack(RFID_FOLDER, rfidTrack);
        playContext = CTX_RFID;
        spinSeed = rfidTrack;
        trackOther(rfidTrack, true);
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
      untrackOther(true);
      playContext = CTX_MUSIC;
      spinSeed = musicTrack;
    } else {
      untrackOther(true);
      rfidTrack = (rfidTrack % totalRfidTracks) + 1;
      Serial.print("[BTN] NEXT → RFID track "); Serial.println(rfidTrack);
      playerPlayFolderTrack(RFID_FOLDER, rfidTrack);
      playContext = CTX_RFID;
      spinSeed = rfidTrack;
      trackOther(rfidTrack, true);
    }
    isPlaying = true;
    applyBrightness();
    randomizeSpinner(spinSeed);
    startSweep(true, CRGB::Cyan);
    prevMode = ANIM_SPINNER;
    showOverlay(OVL_TRACK, browseMode == BROWSE_MUSIC ? musicTrack : rfidTrack);
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
      untrackOther(true);
      playContext = CTX_MUSIC;
      spinSeed = musicTrack;
    } else if (isPlaying && otherPlaying > 0 && currentPosSec() > PREV_RESTART_S) {
      // Like a regular player: past the first seconds, PREV restarts the current track.
      rfidTrack = otherPlaying;
      Serial.print("[BTN] PREV → restart OTHER track "); Serial.println(rfidTrack);
      playerPlayFolderTrack(RFID_FOLDER, rfidTrack);
      playContext = CTX_RFID;
      spinSeed = rfidTrack;
      savePos(rfidTrack, 0);
      trackOther(rfidTrack, false);
    } else {
      untrackOther(true);
      rfidTrack = (rfidTrack - 2 + totalRfidTracks) % totalRfidTracks + 1;
      Serial.print("[BTN] PREV → RFID track "); Serial.println(rfidTrack);
      playerPlayFolderTrack(RFID_FOLDER, rfidTrack);
      playContext = CTX_RFID;
      spinSeed = rfidTrack;
      trackOther(rfidTrack, true);
    }
    isPlaying = true;
    applyBrightness();
    randomizeSpinner(spinSeed);
    startSweep(false, CRGB::Yellow);
    prevMode = ANIM_SPINNER;
    showOverlay(OVL_TRACK, browseMode == BROWSE_MUSIC ? musicTrack : rfidTrack);
  }

  // ── Factory reset: PLAY + NEXT + PREV held for FACTORY_RESET_MS ──
  static uint32_t allHeldSince = 0;
  if (btnPlay.state == LOW && btnNext.state == LOW && btnPrev.state == LOW) {
    seekUsed = true; // suppress the Play action on release
    if (allHeldSince == 0) allHeldSince = millis();
    else if (millis() - allHeldSince >= FACTORY_RESET_MS) factoryReset();
  } else {
    allHeldSince = 0;
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
    static uint32_t lastRfidPoll = 0;
    if (millis() - lastRfidPoll >= 60) {
      lastRfidPoll = millis();
      char rfidPath[32];
      int result = rfidReadPath(rfidPath, sizeof(rfidPath));
      if (result != 0) lastActivityMs = millis();
      if (result == 1) {
        if (isPlaying && playContext == CTX_RFID && strcmp(rfidPath, lastRfidPath) == 0) {
          Serial.println("[RFID] Already playing this track, ignoring");
        } else {
          strncpy(lastRfidPath, rfidPath, sizeof(lastRfidPath));
          playContext = CTX_RFID;
          Serial.print("[RFID] Playing ");
          Serial.println(rfidPath);
          untrackOther(true);
          playerPlayPath(rfidPath);
          if (strncmp(rfidPath, "/" RFID_FOLDER "/", strlen(RFID_FOLDER) + 2) == 0) {
            int n = atoi(rfidPath + strlen(RFID_FOLDER) + 2);
            if (n > 0) trackOther(n, true);
          }
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
  }

  if (isPlaying)
    checkTrackEnd();

  // ── Idle tracking / auto power-off ──
  if (isPlaying || inProgramMode || delta != 0 || playEvent != 0 ||
      nextEvent != 0 || prevEvent != 0 || btnPlay.state == LOW ||
      btnNext.state == LOW || btnPrev.state == LOW)
    lastActivityMs = millis();
  else if (millis() - lastActivityMs >= IDLE_SLEEP_MS)
    goToSleep();

  if (otherPlaying > 0 && isPlaying && millis() - lastPosSaveMs >= POS_SAVE_EVERY_MS) {
    savePos(otherPlaying, currentPosSec());
    lastPosSaveMs = millis();
  }

  saveStateIfChanged();
  updateLEDs();

  // ── WiFi, Captive Portal & WebServer ──
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
  } else if (wifiConnected) {
    server.handleClient();
  }
}
