#include <Arduino.h>
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#else
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

constexpr uint8_t LED_PIN = 2;
constexpr uint8_t BAR_LEN = 10;
constexpr uint16_t PWM_FREQ = 5000;
constexpr uint8_t PWM_BITS = 10;
constexpr uint16_t PWM_MAX = (1 << PWM_BITS) - 1;
constexpr bool BAR_ACTIVE_LOW = false;

constexpr unsigned long FRAME_MS = 20;
constexpr unsigned long MODE_MS = 7000;
constexpr unsigned long FADE_MS = 700;
constexpr float TWO_PI_F = 6.2831853f;

const uint8_t BAR_PINS[BAR_LEN] = {4, 5, 18, 19, 21, 22, 23, 25, 26, 27};

enum EffectMode : uint8_t {
  COMET = 0,
  WAVE,
  BREATH_CENTER,
  TWINKLE,
  BOUNCE_DUAL,
  RAIN,
  EFFECT_COUNT
};

unsigned long lastFrameMs = 0;
unsigned long lastBlinkMs = 0;
unsigned long modeStartMs = 0;
unsigned long lastLogMs = 0;
bool ledState = false;
EffectMode currentMode = COMET;

float phaseA = 0.0f;
float phaseB = 0.0f;
float trailA[BAR_LEN] = {0};
float trailB[BAR_LEN] = {0};
float sparkle[BAR_LEN] = {0};

void writeDuty(uint8_t seg, uint16_t duty) {
  uint16_t outputDuty = BAR_ACTIVE_LOW ? (PWM_MAX - duty) : duty;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BAR_PINS[seg], outputDuty);
#else
  ledcWrite(seg, outputDuty);
#endif
}

void setupPwm() {
  for (uint8_t i = 0; i < BAR_LEN; i++) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(BAR_PINS[i], PWM_FREQ, PWM_BITS, i);
#else
    ledcSetup(i, PWM_FREQ, PWM_BITS);
    ledcAttachPin(BAR_PINS[i], i);
#endif
    writeDuty(i, 0);
  }
}

void clearBuffer(float *buf) {
  for (uint8_t i = 0; i < BAR_LEN; i++) buf[i] = 0.0f;
}

void applyToLeds(const float *values, float gain) {
  for (uint8_t i = 0; i < BAR_LEN; i++) {
    float v = values[i] * gain;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    writeDuty(i, static_cast<uint16_t>(v * PWM_MAX));
  }
}

void renderComet(float t, float *out) {
  clearBuffer(out);
  float p = (sinf(t * 0.90f) * 0.5f + 0.5f) * (BAR_LEN - 1);
  int8_t idx = static_cast<int8_t>(p + 0.5f);
  float dp = cosf(t * 0.90f);
  int8_t dir = (dp >= 0.0f) ? -1 : 1;

  for (uint8_t i = 0; i < BAR_LEN; i++) trailA[i] *= 0.84f;
  if (idx >= 0 && idx < BAR_LEN) trailA[idx] = 1.0f;
  if (idx + dir >= 0 && idx + dir < BAR_LEN) trailA[idx + dir] = max(trailA[idx + dir], 0.54f);
  if (idx + 2 * dir >= 0 && idx + 2 * dir < BAR_LEN) trailA[idx + 2 * dir] = max(trailA[idx + 2 * dir], 0.24f);

  for (uint8_t i = 0; i < BAR_LEN; i++) out[i] = trailA[i];
}

void renderWave(float t, float *out) {
  for (uint8_t i = 0; i < BAR_LEN; i++) {
    float x = t * 1.35f + i * 0.72f;
    float y = (sinf(x) + 1.0f) * 0.5f;
    out[i] = 0.08f + 0.92f * y;
  }
}

void renderBreathCenter(float t, float *out) {
  float breath = (sinf(t * 1.05f) + 1.0f) * 0.5f;
  for (uint8_t i = 0; i < BAR_LEN; i++) {
    float d = fabsf(i - 4.5f) / 4.5f;
    float shape = 1.0f - d;
    if (shape < 0.0f) shape = 0.0f;
    out[i] = 0.05f + shape * (0.2f + 0.75f * breath);
  }
}

void renderTwinkle(float t, float *out) {
  (void)t;
  for (uint8_t i = 0; i < BAR_LEN; i++) {
    sparkle[i] *= 0.90f;
    if (random(0, 100) < 6) sparkle[i] = 1.0f;
    float ambient = 0.03f + 0.04f * sinf(phaseA * 0.35f + i * 0.6f);
    out[i] = ambient + sparkle[i] * 0.95f;
  }
}

void renderBounceDual(float t, float *out) {
  clearBuffer(out);
  float p1 = (sinf(t * 1.25f) * 0.5f + 0.5f) * (BAR_LEN - 1);
  float p2 = (sinf(t * 1.25f + 3.14159f) * 0.5f + 0.5f) * (BAR_LEN - 1);
  for (uint8_t i = 0; i < BAR_LEN; i++) {
    float d1 = fabsf(i - p1);
    float d2 = fabsf(i - p2);
    float g1 = max(0.0f, 1.0f - d1 / 1.6f);
    float g2 = max(0.0f, 1.0f - d2 / 1.6f);
    out[i] = max(g1, g2);
  }
}

void renderRain(float t, float *out) {
  (void)t;
  for (uint8_t i = 0; i < BAR_LEN; i++) trailB[i] *= 0.82f;
  if (random(0, 100) < 20) {
    uint8_t idx = random(0, BAR_LEN);
    trailB[idx] = 1.0f;
  }
  for (uint8_t i = 0; i < BAR_LEN; i++) {
    float side = fabsf(i - 4.5f) / 4.5f;
    float shape = 0.35f + 0.65f * (1.0f - side);
    out[i] = trailB[i] * shape;
  }
}

void renderMode(EffectMode mode, float t, float *out) {
  switch (mode) {
    case COMET:
      renderComet(t, out);
      break;
    case WAVE:
      renderWave(t, out);
      break;
    case BREATH_CENTER:
      renderBreathCenter(t, out);
      break;
    case TWINKLE:
      renderTwinkle(t, out);
      break;
    case BOUNCE_DUAL:
      renderBounceDual(t, out);
      break;
    case RAIN:
      renderRain(t, out);
      break;
    default:
      clearBuffer(out);
      break;
  }
}

const char *modeName(EffectMode mode) {
  switch (mode) {
    case COMET: return "Comet";
    case WAVE: return "Wave";
    case BREATH_CENTER: return "Breath";
    case TWINKLE: return "Twinkle";
    case BOUNCE_DUAL: return "DualBounce";
    case RAIN: return "Rain";
    default: return "Unknown";
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  setupPwm();

  randomSeed(micros());
  modeStartMs = millis();
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("Auto Pattern Cycle");
}

void loop() {
  unsigned long now = millis();

  if (now - lastBlinkMs >= 500) {
    lastBlinkMs = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  if (now - lastFrameMs < FRAME_MS) return;
  lastFrameMs = now;

  if (now - modeStartMs >= MODE_MS) {
    currentMode = static_cast<EffectMode>((currentMode + 1) % EFFECT_COUNT);
    modeStartMs = now;
  }

  float t = now * 0.001f;
  float modeElapsed = static_cast<float>(now - modeStartMs);
  float gain = 1.0f;
  if (modeElapsed < FADE_MS) gain = modeElapsed / FADE_MS;
  if (modeElapsed > (MODE_MS - FADE_MS)) gain = (MODE_MS - modeElapsed) / FADE_MS;
  if (gain < 0.0f) gain = 0.0f;
  if (gain > 1.0f) gain = 1.0f;

  float frame[BAR_LEN] = {0};
  renderMode(currentMode, t, frame);
  phaseA += 0.07f;
  phaseB += 0.05f;
  if (phaseA > TWO_PI_F) phaseA -= TWO_PI_F;
  if (phaseB > TWO_PI_F) phaseB -= TWO_PI_F;
  applyToLeds(frame, gain);

  if (now - lastLogMs >= 400) {
    lastLogMs = now;
    Serial.println(modeName(currentMode));
  }
}
