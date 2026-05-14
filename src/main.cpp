#include <Arduino.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "project_config.h"

struct DeviceConfig {
  String deviceName;
  String wifiSSID;
  String wifiPass;
  String targetURL;
  bool normallyClosed;
};

Preferences preferences;
DNSServer dnsServer;
WebServer webServer(80);
DeviceConfig config;

bool configModeActive = false;
int configBlinkStep = 0;
bool configLedOn = false;
unsigned long nextConfigBlinkAt = 0;

String htmlEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length());
  for (char c : value) {
    switch (c) {
      case '&':
        escaped += F("&amp;");
        break;
      case '<':
        escaped += F("&lt;");
        break;
      case '>':
        escaped += F("&gt;");
        break;
      case '"':
        escaped += F("&quot;");
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}

String jsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 4);
  for (char c : value) {
    switch (c) {
      case '\\':
        escaped += F("\\\\");
        break;
      case '"':
        escaped += F("\\\"");
        break;
      case '\n':
        escaped += F("\\n");
        break;
      case '\r':
        escaped += F("\\r");
        break;
      case '\t':
        escaped += F("\\t");
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}

bool validDeviceName(const String& value) {
  if (value.length() == 0 || value.length() > 20) {
    return false;
  }

  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value.charAt(i);
    if (!isAlphaNumeric(c) && c != '_') {
      return false;
    }
  }

  return true;
}

void writeLed(bool on) {
  digitalWrite(kStatusLedPin, kStatusLedActiveLow ? !on : on);
}

DeviceConfig loadConfig() {
  DeviceConfig loaded;
  preferences.begin("gmc-toggle", true);
  loaded.deviceName = preferences.getString("deviceName", kDefaultDeviceName);
  loaded.wifiSSID = preferences.getString("wifiSSID", "");
  loaded.wifiPass = preferences.getString("wifiPass", "");
  loaded.targetURL = preferences.getString("targetURL", kDefaultTargetUrl);
  loaded.normallyClosed = preferences.getBool("normallyClosed", kNormallyClosed);
  preferences.end();
  return loaded;
}

void saveConfig(const DeviceConfig& value) {
  preferences.begin("gmc-toggle", false);
  preferences.putString("deviceName", value.deviceName);
  preferences.putString("wifiSSID", value.wifiSSID);
  preferences.putString("wifiPass", value.wifiPass);
  preferences.putString("targetURL", value.targetURL);
  preferences.putBool("normallyClosed", value.normallyClosed);
  preferences.end();
}

bool configIsReady(const DeviceConfig& value) {
  return validDeviceName(value.deviceName) && value.wifiSSID.length() > 0 && value.targetURL.startsWith("http");
}

bool readPhysicalClosedRaw() {
  return digitalRead(kReedSwitchPin) == LOW;
}

bool readStablePhysicalClosed() {
  bool last = readPhysicalClosedRaw();
  unsigned long stableSince = millis();
  const unsigned long startedAt = stableSince;

  while (millis() - startedAt < kDebounceTimeoutMs) {
    const bool current = readPhysicalClosedRaw();
    if (current != last) {
      last = current;
      stableSince = millis();
    }

    if (millis() - stableSince >= kDebounceStableMs) {
      return last;
    }

    delay(5);
  }

  return last;
}

int magnetValueFromPhysical(bool physicalClosed) {
  const bool magnetNear = config.normallyClosed ? !physicalClosed : physicalClosed;
  return magnetNear ? 1 : 0;
}

int readStableMagnetValue() {
  return magnetValueFromPhysical(readStablePhysicalClosed());
}

String buildPayload(int value) {
  return String(F("{\"deviceName\":\"")) + jsonEscape(config.deviceName) + F("\",\"deviceType\":\"") +
         kDeviceType + F("\",\"data\":{\"value\":") + value + F("}}");
}

void savePendingValue(int value) {
  preferences.begin("gmc-toggle", false);
  preferences.putBool("pending", true);
  preferences.putInt("pendingValue", value);
  preferences.end();
}

bool loadPendingValue(int& value) {
  preferences.begin("gmc-toggle", true);
  const bool hasPending = preferences.getBool("pending", false);
  value = preferences.getInt("pendingValue", 0);
  preferences.end();
  return hasPending;
}

void clearPendingValue() {
  preferences.begin("gmc-toggle", false);
  preferences.putBool("pending", false);
  preferences.end();
}

void clearReportState() {
  preferences.begin("gmc-toggle", false);
  preferences.putBool("pending", false);
  preferences.putBool("hasLastReported", false);
  preferences.end();
}

void saveLastReportedValue(int value) {
  preferences.begin("gmc-toggle", false);
  preferences.putBool("hasLastReported", true);
  preferences.putInt("lastReportedValue", value);
  preferences.end();
}

bool loadLastReportedValue(int& value) {
  preferences.begin("gmc-toggle", true);
  const bool hasLastReported = preferences.getBool("hasLastReported", false);
  value = preferences.getInt("lastReportedValue", 0);
  preferences.end();
  return hasLastReported;
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(config.wifiSSID.c_str(), config.wifiPass.c_str());

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < kWifiConnectTimeoutMs) {
    delay(100);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool postValueOnce(int value) {
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);

  if (!http.begin(config.targetURL)) {
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(buildPayload(value));
  http.end();

  return code >= 200 && code < 300;
}

bool postValue(int value) {
  for (int attempt = 0; attempt < kHttpPostAttempts; ++attempt) {
    if (postValueOnce(value)) {
      return true;
    }
    delay(200);
  }

  return false;
}

void armWakeAndSleepFromPhysical(bool finalPhysicalClosed, bool hasPending) {
  const esp_deepsleep_gpio_wake_up_mode_t wakeMode =
      finalPhysicalClosed ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW;

  Serial.print("Final physical reed state: ");
  Serial.println(finalPhysicalClosed ? "closed" : "open");
  Serial.print("Sleeping until reed goes ");
  Serial.println(finalPhysicalClosed ? "open" : "closed");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_deep_sleep_enable_gpio_wakeup(static_cast<gpio_num_t>(kReedSwitchPin), wakeMode);
  if (hasPending) {
    esp_sleep_enable_timer_wakeup(kPendingRetrySleepSeconds * 1000000ULL);
  }

  delay(100);
  esp_deep_sleep_start();
}

void armWakeAndSleep(bool hasPending) {
  armWakeAndSleepFromPhysical(readStablePhysicalClosed(), hasPending);
}

String configPage(const String& message = "") {
  const String checked = config.normallyClosed ? F(" checked") : F("");
  String html;
  html.reserve(4200);
  html += F("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">");
  html += F("<title>GMC Toggle</title><style>");
  html += F(":root{color-scheme:light;--bg:#f6f7f9;--panel:#fff;--ink:#15171a;--muted:#68707a;--line:#d9dde3;--accent:#0b7cff}");
  html += F("*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}");
  html += F("main{width:min(100%,440px);margin:0 auto;padding:22px 16px 34px}h1{font-size:28px;line-height:1.1;margin:8px 0 22px}");
  html += F("form{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:16px;box-shadow:0 8px 24px rgba(20,25,35,.08)}");
  html += F("label{display:block;font-size:13px;font-weight:700;margin:14px 0 7px}input{width:100%;height:46px;border:1px solid var(--line);border-radius:8px;padding:0 12px;font-size:16px;background:#fff;color:var(--ink)}");
  html += F(".row{display:flex;align-items:center;justify-content:space-between;gap:16px;margin:18px 0}.row span{font-size:15px;font-weight:700}");
  html += F("input[type=checkbox]{width:24px;height:24px;accent-color:var(--accent)}button{width:100%;height:48px;border:0;border-radius:8px;background:var(--accent);color:white;font-size:17px;font-weight:800;margin-top:8px}");
  html += F(".msg{background:#e9f3ff;border:1px solid #b7d8ff;border-radius:8px;padding:10px 12px;margin-bottom:14px;color:#124c8c}.hint{color:var(--muted);font-size:12px;margin-top:6px}");
  html += F("</style></head><body><main><h1>GMC Toggle</h1>");
  if (message.length() > 0) {
    html += F("<div class=\"msg\">");
    html += htmlEscape(message);
    html += F("</div>");
  }
  html += F("<form method=\"post\" action=\"/save\">");
  html += F("<label for=\"deviceName\">Device name</label><input id=\"deviceName\" name=\"deviceName\" maxlength=\"20\" pattern=\"[A-Za-z0-9_]{1,20}\" required value=\"");
  html += htmlEscape(config.deviceName);
  html += F("\"><div class=\"hint\">Letters, numbers, underscores.</div>");
  html += F("<label for=\"wifiSSID\">WiFi SSID</label><input id=\"wifiSSID\" name=\"wifiSSID\" required value=\"");
  html += htmlEscape(config.wifiSSID);
  html += F("\"><label for=\"wifiPass\">WiFi password</label><input id=\"wifiPass\" name=\"wifiPass\" type=\"password\" value=\"");
  html += htmlEscape(config.wifiPass);
  html += F("\"><label for=\"targetURL\">Target URL</label><input id=\"targetURL\" name=\"targetURL\" type=\"url\" required value=\"");
  html += htmlEscape(config.targetURL);
  html += F("\"><label class=\"row\" for=\"normallyClosed\"><span>Normally closed</span><input id=\"normallyClosed\" name=\"normallyClosed\" type=\"checkbox\" value=\"1\"");
  html += checked;
  html += F("></label><button type=\"submit\">Save</button></form></main></body></html>");
  return html;
}

void redirectToPortal() {
  webServer.sendHeader("Location", String(F("http://")) + WiFi.softAPIP().toString(), true);
  webServer.send(302, "text/plain", "");
}

void handleSave() {
  DeviceConfig next;
  next.deviceName = webServer.arg("deviceName");
  next.wifiSSID = webServer.arg("wifiSSID");
  next.wifiPass = webServer.arg("wifiPass");
  next.targetURL = webServer.arg("targetURL");
  next.normallyClosed = webServer.hasArg("normallyClosed");

  if (!validDeviceName(next.deviceName) || next.wifiSSID.length() == 0 || !next.targetURL.startsWith("http")) {
    webServer.send(400, "text/html", configPage("Check the saved values."));
    return;
  }

  saveConfig(next);
  clearReportState();
  webServer.send(200, "text/html",
                 F("<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;display:grid;place-items:center;min-height:100vh;margin:0}main{text-align:center;padding:24px}h1{font-size:28px}</style></head><body><main><h1>Saved</h1></main></body></html>"));
  delay(700);
  ESP.restart();
}

void serviceConfigBlink() {
  if (millis() < nextConfigBlinkAt) {
    return;
  }

  if (configBlinkStep < 6) {
    configLedOn = !configLedOn;
    writeLed(configLedOn);
    ++configBlinkStep;
    nextConfigBlinkAt = millis() + (configLedOn ? kConfigLedOnMs : kConfigLedBetweenBlinkMs);
    return;
  }

  writeLed(false);
  configLedOn = false;
  configBlinkStep = 0;
  nextConfigBlinkAt = millis() + kConfigLedRepeatGapMs;
}

void startConfigMode(const char* reason) {
  configModeActive = true;
  Serial.print("Config mode: ");
  Serial.println(reason);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(kConfigApSsid);
  delay(150);

  const IPAddress apIp = WiFi.softAPIP();
  dnsServer.start(53, "*", apIp);

  webServer.on("/", HTTP_GET, []() { webServer.send(200, "text/html", configPage()); });
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.on("/generate_204", HTTP_GET, redirectToPortal);
  webServer.on("/hotspot-detect.html", HTTP_GET, []() { webServer.send(200, "text/html", configPage()); });
  webServer.onNotFound(redirectToPortal);
  webServer.begin();

  Serial.print("AP SSID: ");
  Serial.println(kConfigApSsid);
  Serial.print("AP IP: ");
  Serial.println(apIp);
}

void runNormalMode() {
  int pendingValue = 0;
  const bool hadPending = loadPendingValue(pendingValue);
  int currentValue = readStableMagnetValue();

  Serial.print("Initial magnet value: ");
  Serial.println(currentValue);

  int lastReportedValue = 0;
  const bool hasLastReported = loadLastReportedValue(lastReportedValue);
  if (!hadPending && hasLastReported && currentValue == lastReportedValue) {
    Serial.println("Current value already confirmed by server; sleeping");
    armWakeAndSleep(false);
  }

  if (!connectWifi()) {
    Serial.println("WiFi failed; saving pending state");
    currentValue = readStableMagnetValue();
    savePendingValue(currentValue);
    armWakeAndSleep(true);
  }

  currentValue = readStableMagnetValue();

  if (hadPending && pendingValue != currentValue) {
    Serial.print("Reporting pending value: ");
    Serial.println(pendingValue);
    if (!postValue(pendingValue)) {
      savePendingValue(currentValue);
      armWakeAndSleep(true);
    }
    saveLastReportedValue(pendingValue);
  }

  while (true) {
    currentValue = readStableMagnetValue();
    Serial.print("Reporting current value: ");
    Serial.println(currentValue);

    if (!postValue(currentValue)) {
      Serial.println("HTTP failed; saving pending state");
      savePendingValue(readStableMagnetValue());
      armWakeAndSleep(true);
    }

    saveLastReportedValue(currentValue);
    clearPendingValue();

    delay(kReportRecheckDelayMs);
    const int afterReportValue = readStableMagnetValue();
    if (afterReportValue != currentValue) {
      continue;
    }

    const bool finalPhysicalClosed = readStablePhysicalClosed();
    const int finalValue = magnetValueFromPhysical(finalPhysicalClosed);
    if (finalValue == currentValue) {
      armWakeAndSleepFromPhysical(finalPhysicalClosed, false);
    }

    Serial.println("State changed during final pre-sleep check; reporting again");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(kReedSwitchPin, INPUT_PULLUP);
  pinMode(kConfigButtonPin, INPUT_PULLUP);
  pinMode(kStatusLedPin, OUTPUT);
  writeLed(false);

  config = loadConfig();

  Serial.println();
  Serial.println("GMC Toggle");
  Serial.print("Reed GPIO: ");
  Serial.println(kReedSwitchPin);
  Serial.print("Config button GPIO: ");
  Serial.println(kConfigButtonPin);
  Serial.print("Reed contact type: ");
  Serial.println(config.normallyClosed ? "NC" : "NO");
  Serial.print("Config AP SSID: ");
  Serial.println(kConfigApSsid);

  if (digitalRead(kConfigButtonPin) == LOW) {
    startConfigMode("button held at boot");
    return;
  }

  if (!configIsReady(config)) {
    startConfigMode("missing configuration");
    return;
  }

  runNormalMode();
}

void loop() {
  if (configModeActive) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    serviceConfigBlink();
  }
}
