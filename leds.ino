// ── LED animations ────────────────────────────────────────────────────────────

uint32_t lastLedMs = 0;

// Breathe state
uint8_t breathVal = 5;
int8_t breathDir = 1;

// Spinner state (randomized per playback)
uint8_t waveHue = 0;
bool spinnerCW = true;
uint8_t spinnerHueBase = 0;

// Sweep state
uint8_t sweepPos = 0;
bool sweepCW = true;
CRGB sweepColor;

// Flash state
uint8_t flashTicks = 0;
CRGB flashColor;

// Program mode state
uint8_t programPos = 0;

// Program-success state
uint8_t progSuccessStep = 0;

void ledSetup() {
  FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setDither(BINARY_DITHER);
  FastLED.setBrightness(LED_BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void stepBootAnimation() {
  static uint8_t bootPos = 0;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int t = 0; t < 5; t++) {
    int idx = (bootPos - t + NUM_LEDS) % NUM_LEDS;
    leds[idx] = CRGB(0, 180, 255);
    leds[idx].nscale8(255 - t * 48);
  }
  FastLED.show();
  bootPos = (bootPos + 1) % NUM_LEDS;
}

void delayWithBootAnimation(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    stepBootAnimation();
    delay(40);
  }
}

// Pick direction and base color for the next spinner run, deterministically
// from `seed` so the same track always looks the same.
// Music: any starting offset on the rainbow.
// RFID:  a jewel-tone hue (green→cyan→blue→purple→pink→magenta).
void randomizeSpinner(uint32_t seed) {
  spinnerCW = (seed & 1) == 0;
  if (playContext == CTX_MUSIC) {
    // Curated bright/saturated hues — warm + cool, but never "all colors at once".
    static const uint8_t palette[] = {
       0,   // red
      20,   // orange
      40,   // amber
      64,   // yellow-green
      96,   // green
     128,   // cyan
     160,   // sky blue
     192,   // purple
     220,   // pink
     240,   // magenta
    };
    spinnerHueBase = palette[(seed >> 1) % sizeof(palette)];
  } else {
    static const uint8_t palette[] = {96, 128, 160, 192, 220, 240};
    spinnerHueBase = palette[(seed >> 1) % sizeof(palette)];
  }
  waveHue = 0;
  lastLedMs = 0;
  Serial.print("[SPIN] ctx=");
  Serial.print(playContext == CTX_MUSIC ? "MUSIC" : "RFID");
  Serial.print(" seed=");
  Serial.print(seed);
  Serial.print(" dir=");
  Serial.print(spinnerCW ? "CW" : "CCW");
  Serial.print(" hue=");
  Serial.println(spinnerHueBase);
}

// Advance the seek comet one step per encoder tick.
void stepSeekSweep(bool cw) {
  if (animMode != ANIM_SEEK) {
    prevMode = ANIM_SPINNER;
    animMode = ANIM_SEEK;
    sweepPos = 0; // start from consistent position
  }
  sweepCW = cw;
  sweepColor = cw ? CRGB::Cyan : CRGB::Yellow;
  sweepPos = cw ? (sweepPos + 1) % NUM_LEDS
                : (sweepPos - 1 + NUM_LEDS) % NUM_LEDS;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int t = 0; t < 4; t++) {
    int idx = cw ? (sweepPos - t + NUM_LEDS) % NUM_LEDS
                 : (sweepPos + t) % NUM_LEDS;
    leds[idx] = sweepColor;
    leds[idx].nscale8(255 - t * 55);
  }
  FastLED.show();
}

void startFlash(CRGB color) {
  prevMode = (animMode == ANIM_FLASH) ? prevMode : animMode;
  animMode = ANIM_FLASH;
  flashColor = color;
  flashTicks = 0;
  lastLedMs = 0; // render first frame immediately
}

void startProgramSuccess() {
  animMode = ANIM_PROGRAM_SUCCESS;
  progSuccessStep = 0;
  lastLedMs = 0;
}

void startSweep(bool cw, CRGB color) {
  prevMode = (animMode == ANIM_SPINNER || animMode == ANIM_BREATHE) ? animMode
                                                                    : prevMode;
  animMode = cw ? ANIM_SWEEP_CW : ANIM_SWEEP_CCW;
  sweepCW = cw;
  sweepColor = color;
  sweepPos = cw ? 0 : (NUM_LEDS - 1);
  lastLedMs = 0; // render first frame immediately
}

// ── Overlays: temporary static displays drawn over the animations ────────────
// OVL_VOLUME / OVL_LED: arc of 1..16 LEDs proportional to the value.
// OVL_TRACK: track number as dots. Track 1..16 → that many LEDs lit in ring
// colour #0; every further 16 tracks start a new ring with the next colour,
// the previous ring's colour stays on the unlit remainder. E.g. track 17 =
// 1 LED colour #1 + 15 LEDs colour #0. Adjacent colours (incl. wrap 9→0) are
// strongly contrasting.
static const CRGB TRACK_RING_COLORS[10] = {
  CRGB(255,   0,   0),  // 0 red
  CRGB(  0, 255, 255),  // 1 cyan
  CRGB(255, 200,   0),  // 2 yellow
  CRGB(  0,   0, 255),  // 3 blue
  CRGB(  0, 255,   0),  // 4 green
  CRGB(255,   0, 255),  // 5 magenta
  CRGB(255,  90,   0),  // 6 orange
  CRGB(255, 255, 255),  // 7 white
  CRGB(130,   0, 255),  // 8 violet
  CRGB(160, 255,   0),  // 9 lime
};

int overlayKind = 0;
int overlayValue = 0;
uint32_t overlayUntil = 0;
uint32_t overlayLastMs = 0;

void showOverlay(int kind, int value) {
  overlayKind = kind;
  overlayValue = value;
  overlayUntil = millis() + (kind == OVL_TRACK ? OVERLAY_TRACK_MS : OVERLAY_VALUE_MS);
  overlayLastMs = 0;
}

static void renderOverlay() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (overlayKind == OVL_TRACK) {
    int t = max(overlayValue, 1) - 1;          // 0-based
    int ring = t / NUM_LEDS;                   // which ring of 16
    int lit = t % NUM_LEDS + 1;                // dots in the current ring
    CRGB cur  = TRACK_RING_COLORS[ring % 10];
    CRGB prev = TRACK_RING_COLORS[(ring + 9) % 10];
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = i < lit ? cur : (ring > 0 ? prev : CRGB(CRGB::Black));
  } else {
    int n = 0;
    CRGB c;
    if (overlayKind == OVL_VOLUME) {
      n = 1 + (overlayValue - VOLUME_MIN) * (NUM_LEDS - 1) / (VOLUME_MAX - VOLUME_MIN);
      c = CRGB(0, 160, 255);                   // blue arc = volume
    } else {
      n = overlayValue * NUM_LEDS / LED_LEVEL_MAX;
      c = CRGB(255, 170, 40);                  // warm arc = LED brightness
    }
    for (int i = 0; i < n && i < NUM_LEDS; i++) leds[i] = c;
  }
  FastLED.show();
}

void updateLEDs() {
  uint32_t now = millis();

  if (overlayKind) {
    if ((int32_t)(now - overlayUntil) >= 0) {
      overlayKind = 0;
      lastLedMs = 0; // let the animation redraw immediately
    } else {
      if (now - overlayLastMs >= 30) { overlayLastMs = now; renderOverlay(); }
      return;
    }
  }

  switch (animMode) {

  case ANIM_BREATHE: {
    if (now - lastLedMs < 10) // 100 FPS refresh for smooth FastLED temporal dithering
      break;
    lastLedMs = now;
    uint8_t rawSine = beatsin8(14, 15, 255);
    uint8_t breathVal = dim8_video(rawSine);
    fill_solid(leds, NUM_LEDS, CHSV(96, 220, breathVal));
    FastLED.show();
    break;
  }

  case ANIM_SPINNER: {
    // Full-ring two-tone gradient rotating smoothly around the ring.
    // A cos wave modulates hue ±amp around spinnerHueBase, so the ring shows
    // a narrow band of related colours (never the full rainbow) and the
    // pattern wraps seamlessly.
    if (now - lastLedMs < (playContext == CTX_MUSIC ? 35 : 50))
      break;
    lastLedMs = now;
    int8_t  dir = spinnerCW ? 1 : -1;
    uint8_t amp = (playContext == CTX_MUSIC ? 88 : 56);
    for (int i = 0; i < NUM_LEDS; i++) {
      uint8_t angle  = (uint8_t)(i * (256 / NUM_LEDS) + (int)waveHue * dir);
      int16_t offset = ((int16_t)cos8(angle) - 128) * amp / 128;
      leds[i] = CHSV(spinnerHueBase + offset, 230, 255);
    }
    waveHue += 2;
    FastLED.show();
    break;
  }

  case ANIM_SWEEP_CW:
  case ANIM_SWEEP_CCW:
    if (now - lastLedMs < 35)
      break;
    lastLedMs = now;
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int t = 0; t < 4; t++) {
      int idx = sweepCW ? (sweepPos - t + NUM_LEDS) % NUM_LEDS
                        : (sweepPos + t) % NUM_LEDS;
      leds[idx] = sweepColor;
      leds[idx].nscale8(255 - t * 55);
    }
    FastLED.show();
    sweepPos = sweepCW ? (sweepPos + 1) % NUM_LEDS
                       : (sweepPos - 1 + NUM_LEDS) % NUM_LEDS;
    if (sweepCW && sweepPos == 0)
      animMode = prevMode;
    if (!sweepCW && sweepPos == NUM_LEDS - 1)
      animMode = prevMode;
    break;

  case ANIM_SEEK:
    // position is driven by the encoder — nothing to auto-advance here
    break;

  case ANIM_PROGRAM:
    if (now - lastLedMs < 40)
      break;
    lastLedMs = now;
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    // two opposing purple comets
    for (int t = 0; t < 3; t++) {
      uint8_t bright = 255 - t * 80;
      leds[(programPos - t + NUM_LEDS) % NUM_LEDS] = CHSV(192, 255, bright);
      leds[(programPos + NUM_LEDS / 2 - t + NUM_LEDS) % NUM_LEDS] = CHSV(192, 255, bright);
    }
    programPos = (programPos + 1) % NUM_LEDS;
    FastLED.show();
    break;

  case ANIM_PROGRAM_SUCCESS:
    if (now - lastLedMs < 35)
      break;
    lastLedMs = now;
    if (progSuccessStep < NUM_LEDS) {
      // Sweep green progressively around the ring CW.
      for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = (i <= progSuccessStep) ? CRGB::Green : CRGB::Black;
    } else {
      // Hold full green.
      fill_solid(leds, NUM_LEDS, CRGB::Green);
    }
    FastLED.show();
    if (++progSuccessStep >= NUM_LEDS + 10)
      animMode = prevMode;
    break;

  case ANIM_FLASH:
    if (now - lastLedMs < 75)
      break;
    lastLedMs = now;
    fill_solid(leds, NUM_LEDS, flashTicks % 2 == 0 ? flashColor : CRGB::Black);
    FastLED.show();
    if (++flashTicks >= 6)
      animMode = prevMode;
    break;
  }
}
