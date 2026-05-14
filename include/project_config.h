#pragma once

// ESP32-C3 deep-sleep GPIO wake is available on GPIO0 through GPIO5.
constexpr int kReedSwitchPin = 3;
constexpr int kConfigButtonPin = 4;
constexpr int kStatusLedPin = 8;

constexpr bool kNormallyClosed = false;
constexpr bool kStatusLedActiveLow = true;

constexpr char kDeviceType[] = "toggle";
constexpr char kConfigApSsid[] = "GMC Toggle";
constexpr char kDefaultDeviceName[] = "toggle_";
constexpr char kDefaultTargetUrl[] = "http://192.168.0.100/trigger.php";

constexpr unsigned long kConfigLedOnMs = 150;
constexpr unsigned long kConfigLedBetweenBlinkMs = 150;
constexpr unsigned long kConfigLedRepeatGapMs = 2000;
constexpr unsigned long kWifiConnectTimeoutMs = 15000;
constexpr unsigned long kHttpTimeoutMs = 3000;
constexpr unsigned long kDebounceStableMs = 50;
constexpr unsigned long kDebounceTimeoutMs = 500;
constexpr unsigned long kReportRecheckDelayMs = 40;

constexpr int kHttpPostAttempts = 3;
constexpr uint64_t kPendingRetrySleepSeconds = 300;
