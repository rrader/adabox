// ── LED animations ────────────────────────────────────────────────────────────

uint32_t lastLedMs = 0;

// Breathe state
uint8_t breathVal = 5;
int8_t breathDir = 1;

// Color wave state
uint8_t waveHue = 0;

// Sweep state
uint8_t sweepPos = 0;
bool sweepCW = true;
CRGB sweepColor;

// Flash state
uint8_t flashTicks = 0;
CRGB flashColor;

// Program mode state
uint8_t programPos = 0;

void ledSetup() {
  FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
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
    fill_solid(leds, NUM_LEDS, CHSV(160, 220, breathVal)); // blue
    FastLED.show();
    break;

  case ANIM_SPINNER:
    if (now - lastLedMs < 30)
      break;
    lastLedMs = now;
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CHSV(waveHue + i * (256 / NUM_LEDS), 240, 255);
    waveHue += 2;
    FastLED.show();
    break;

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
