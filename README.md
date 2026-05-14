# GMC Toggle

Clean-slate ESP32-C3 reed switch notifier.

This project intentionally does not reuse code from earlier attempts. It is a fresh PlatformIO firmware project plus a small PHP endpoint/viewer for a local LAN web server.

## Scope

The ESP32-mini-C3 monitors a reed switch and reports state changes to a configured local HTTP target.

Normal runtime:

1. Sleep until the reed switch changes state.
2. Wake up and record the current reed state.
3. Connect to the configured WiFi network.
4. POST JSON to `targetURL`.
5. Re-check the reed switch before sleeping.
6. If the reed state changed during reporting, send the updated state as another JSON event.
7. Sleep again and wake only when the inverse of the current reed state occurs.

Configuration mode:

1. A separate two-wire button enters config mode.
2. Normal reed reporting is ignored while in config mode.
3. The ESP32 starts a password-free WiFi access point.
4. A captive portal opens a device configuration page.
5. The configuration page stores:
   - `deviceName`: max 20 characters, letters, numbers, and underscores only.
   - `wifiSSID`: normal WiFi SSID used outside config mode.
   - `wifiPass`: normal WiFi password.
   - `targetURL`: local web server endpoint, for example `http://192.168.0.78/trigger.php`.
   - `reedSwitchNormallyClosed`: whether the installed reed switch is NC instead of NO.
   - `onMeansMagnetPresent`: whether the normalized `ON` state means the magnet is present.

## HTTP Folder

The `http` folder contains files intended to be copied to the local LAN web server.

- `http/trigger.php` accepts JSON events and writes the latest state to `http/data/<deviceName>.json`.
- `http/index.php` displays the latest known state as plain text and refreshes automatically.

## JSON Contract

Firmware will POST JSON like this:

```json
{
  "deviceName": "garage_door",
  "reedState": "ON",
  "reedClosed": true,
  "millis": 12345
}
```

`reedState` is the normalized human-readable state shown by the PHP page. `reedClosed` is the physical switch reading after input debouncing, before NO/NC meaning is applied.

## Reed Switch Normalization

The wake source is based only on the physical GPIO level at the moment the ESP32 goes to sleep. After waking, the firmware will normalize the physical reed reading using the configured switch type before sending status to the server.

- NO switch: closed means magnet present.
- NC switch: open means magnet present.
- `onMeansMagnetPresent` decides whether magnet present is reported as `ON` or `OFF`.

This lets each device correct for NO or NC installations locally while the server receives a consistent final state.

## Recommended Input Hardware

For reliable reed readings, wire the reed switch between the reed GPIO and GND, with the GPIO pulled up.

Recommended starting values:

- External pullup from reed GPIO to 3.3 V: 100 kOhm for reliability, or 330 kOhm to 1 MOhm if battery current is more important.
- Debounce/noise capacitor from reed GPIO to GND: 10 nF to 100 nF, placed close to the ESP32 board.
- Optional series resistor between reed wire and GPIO: 100 Ohm to 1 kOhm, especially if the reed wire leaves the enclosure or runs near noisy wiring.
- Local supply decoupling near the ESP32: 100 nF ceramic plus 220 uF to 470 uF bulk capacitance.

The firmware should still debounce in software after waking. Hardware filtering is there to reduce false wakes and noisy edges, not to replace firmware confirmation.

For each sleep cycle, the firmware should configure wake for the inverse of the final physical GPIO level. NO/NC correction only affects the reported `reedState`, not the wake edge selection.

## Hardware Decisions Still Needed

Before final firmware pin behavior is locked in, choose:

- Confirm that GPIO3 and GPIO4 match your ESP32-mini-C3 board's physical pinout. They are chosen because common ESP32-C3 Super Mini layouts place GPIO0-GPIO4 on the same side as GND, while GPIO5 is on the opposite side.
- Whether each input is wired to ground using internal pullups, or wired another way.
- Whether `ON` should mean magnet present or magnet absent for your installation.

Current placeholder defaults live in [include/project_config.h](include/project_config.h).
