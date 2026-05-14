#include <Arduino.h>

#include "project_config.h"

void setup() {
  Serial.begin(115200);
  delay(300);

  const uint8_t inputMode = kUseInputPullups ? INPUT_PULLUP : INPUT;
  pinMode(kReedSwitchPin, inputMode);
  pinMode(kConfigButtonPin, inputMode);

  Serial.println();
  Serial.println("GMC Toggle clean-slate firmware scaffold");
  Serial.print("Reed GPIO: ");
  Serial.println(kReedSwitchPin);
  Serial.print("Config button GPIO: ");
  Serial.println(kConfigButtonPin);
  Serial.print("Reed contact type: ");
  Serial.println(kReedSwitchNormallyClosed ? "NC" : "NO");
}

void loop() {
  // Firmware behavior will be added after the scope and GPIO wiring are confirmed.
  delay(1000);
}
