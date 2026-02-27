#include <Arduino.h>
#include <MavistraController.h>

MavistraController controller("Mavistra Demo bot");

static constexpr uint8_t kBuzzerPin = 15;

struct LedMapping {
  uint8_t     pin;
  const char* command;
};

static const LedMapping kLeds[] = {
    {4,  "L_UP"},
    {5,  "L_DOWN"},
    {18, "L_LEFT"},
    {19, "L_RIGHT"},
    {22, "L_CENTER"},
    {23, "R_UP"},
    {25, "R_DOWN"},
    {26, "R_LEFT"},
    {27, "R_RIGHT"},
    {21, "R_CENTER"},
};
static constexpr uint8_t kLedCount = sizeof(kLeds) / sizeof(kLeds[0]);

// C major scale across two octaves, one note per LED.
static const uint16_t kScaleAsc[]  = {262, 294, 330, 349, 392, 440, 494, 523, 587, 659};
static const uint16_t kScaleDesc[] = {659, 587, 523, 494, 440, 392, 349, 330, 294, 262};

// Bit-bang a tone on the buzzer pin for durationMs.
// Works without any LEDC/PWM setup — safe to call any time.
static void buzzerTone(uint16_t freq, uint16_t durationMs) {
  const uint32_t halfPeriodUs = 500000UL / freq;
  const uint32_t cycles       = static_cast<uint32_t>(durationMs) * freq / 1000UL;
  for (uint32_t i = 0U; i < cycles; i++) {
    digitalWrite(kBuzzerPin, HIGH);
    delayMicroseconds(halfPeriodUs);
    digitalWrite(kBuzzerPin, LOW);
    delayMicroseconds(halfPeriodUs);
  }
}

void animateBoot() {
  // LEDs chase left to right across three rising notes, then all flash off.
  static const uint16_t kNotes[]     = {262, 392, 523};  // C4 G4 C5
  static const uint16_t kDurations[] = {120, 120, 200};

  uint8_t led = 0U;
  for (uint8_t n = 0U; n < 3U; n++) {
    const uint8_t ledsThisNote = (n < 2U) ? (kLedCount / 3U) : (kLedCount - led);
    const uint16_t stepMs      = kDurations[n] / ledsThisNote;
    for (uint8_t k = 0U; k < ledsThisNote && led < kLedCount; k++, led++) {
      digitalWrite(kLeds[led].pin, HIGH);
      buzzerTone(kNotes[n], stepMs);
    }
  }

  delay(100);
  for (uint8_t i = 0U; i < kLedCount; i++) {
    digitalWrite(kLeds[i].pin, LOW);
  }
  delay(150);
}

void animateConnect() {
  // LEDs fill left to right, ascending tone sweep.
  for (uint8_t i = 0U; i < kLedCount; i++) {
    digitalWrite(kLeds[i].pin, HIGH);
    buzzerTone(kScaleAsc[i], 70);
  }
}

void animateDisconnect() {
  // LEDs wipe right to left, descending tone sweep.
  for (int8_t i = static_cast<int8_t>(kLedCount) - 1; i >= 0; i--) {
    digitalWrite(kLeds[i].pin, LOW);
    buzzerTone(kScaleDesc[kLedCount - 1U - static_cast<uint8_t>(i)], 70);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(kBuzzerPin, OUTPUT);
  for (uint8_t i = 0U; i < kLedCount; i++) {
    pinMode(kLeds[i].pin, OUTPUT);
    digitalWrite(kLeds[i].pin, LOW);
  }

  animateBoot();

  controller.begin();
}

void loop() {
  controller.loop();

  static bool prevConnected = false;
  const bool  connected     = controller.isConnected();

  if (connected != prevConnected) {
    connected ? animateConnect() : animateDisconnect();
    prevConnected = connected;
  }

  if (connected) {
    for (uint8_t i = 0U; i < kLedCount; i++) {
      digitalWrite(kLeds[i].pin,
                   controller.isActive(kLeds[i].command) ? HIGH : LOW);
    }
  }
}
