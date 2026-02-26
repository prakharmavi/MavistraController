#include <Arduino.h>
#include <MavistraController.h>

MavistraController controller("Vanshaj's Bot");

// Print which commands are currently active every 200 ms.
static constexpr const char* kCommands[] = {
    "L_UP", "L_DOWN", "L_LEFT", "L_RIGHT", "L_CENTER",
    "R_UP", "R_DOWN", "R_LEFT", "R_RIGHT", "R_CENTER",
    "BTN_1", "BTN_2", "BTN_3", "BTN_4",
};
static constexpr uint8_t kCommandCount =
    sizeof(kCommands) / sizeof(kCommands[0]);

void setup() {
  Serial.begin(115200);
  delay(300);
  controller.setCommandTimeout(300);  // 3 s for manual BLE testing
  controller.begin();
}

void loop() {
  controller.loop();

  static uint32_t lastPrintMs = 0;
  const uint32_t now = millis();
  if (now - lastPrintMs >= 200U) {
    lastPrintMs = now;
    bool any = false;
    for (uint8_t i = 0U; i < kCommandCount; i++) {
      if (controller.isActive(kCommands[i])) {
        Serial.print(kCommands[i]);
        Serial.print(' ');
        any = true;
      }
    }
    if (any) {
      Serial.println();
    }
  }
}
