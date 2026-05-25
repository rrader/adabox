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
  FastLED.setBrightness(LED_BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
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

void updateLEDs() {
  uint32_t now = millis();

  switch (animMode) {

  case ANIM_BREATHE:
    if (now - lastLedMs < 14)
      break;
    lastLedMs = now;
    breathVal = constrain((int)breathVal + breathDir * 3, 5, 180);
    if (breathVal >= 180)
      breathDir = -1;
    if (breathVal <= 5)
      breathDir = 1;
    fill_solid(leds, NUM_LEDS, CHSV(96, 220, breathVal)); // green idle
    FastLED.show();
    break;

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
