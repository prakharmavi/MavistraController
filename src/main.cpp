#include <Arduino.h>
#include <MavistraController.h>

MavistraController controller("MavistraCtrl");

void setup() {
  Serial.begin(115200);
  delay(300);
  controller.begin();
}

void loop() {
  controller.loop();
}
