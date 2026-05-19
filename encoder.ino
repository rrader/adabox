// ── Rotary encoder KY-040 / EC11 ─────────────────────────────────────────────
// CLK → GPIO 18,  DT → GPIO 19  (SW pin unused)

#define PIN_ENC_CLK 18
#define PIN_ENC_DT  19

volatile int encoderDelta = 0;

// Counts valid quadrature transitions only — ignores contact bounce.
void IRAM_ATTR encoderISR() {
  static uint8_t prev = 0b11;
  uint8_t curr = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  if      (prev == 0b10 && curr == 0b00) encoderDelta--;
  else if (prev == 0b01 && curr == 0b00) encoderDelta++;
  prev = curr;
}

void encoderSetup() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT,  INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT),  encoderISR, CHANGE);
}

// Returns accumulated ticks since last call and resets the counter.
int drainEncoder() {
  if (encoderDelta == 0) return 0;
  noInterrupts();
  int d = encoderDelta;
  encoderDelta = 0;
  interrupts();
  return d;
}
