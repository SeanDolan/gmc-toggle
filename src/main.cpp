#include <Arduino.h>

#include "project_config.h"

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(kReedSwitchPin, INPUT_PULLUP);
  pinMode(kConfigButtonPin, INPUT_PULLUP);

  Serial.println();
  Serial.println("GMC Toggle clean-slate firmware scaffold");
  Serial.print("Reed GPIO: ");
  Serial.println(kReedSwitchPin);
  Serial.print("Config button GPIO: ");
  Serial.println(kConfigButtonPin);
  Serial.print("Reed contact type: ");
  Serial.println(kNormallyClosed ? "NC" : "NO");
  Serial.print("Config AP SSID: ");
  Serial.println(kConfigApSsid);
}

void loop() {
  // Firmware behavior will be added after the scope and GPIO wiring are confirmed.
  delay(1000);
}
