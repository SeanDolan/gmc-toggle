#pragma once

// Clean-slate hardware defaults. Adjust these once the final wiring is chosen.
// ESP32-C3 deep-sleep GPIO wake is available on GPIO0 through GPIO5.
constexpr int kReedSwitchPin = 3;
constexpr int kConfigButtonPin = 4;

constexpr bool kUseInputPullups = true;
constexpr bool kReedSwitchNormallyClosed = false;
constexpr bool kOnMeansMagnetPresent = true;

constexpr char kConfigApSsid[] = "GMC-Toggle-Setup";
constexpr unsigned long kWifiConnectTimeoutMs = 15000;
constexpr unsigned long kHttpTimeoutMs = 8000;
