# GMC Toggle

Clean-slate ESP32-C3 reed switch notifier.

This project intentionally does not reuse code from earlier attempts. It is a fresh PlatformIO firmware project plus a small PHP endpoint/viewer for a local LAN web server.

## Scope

The ESP32-mini-C3 monitors a reed switch and reports state changes to a configured local HTTP target.

Normal runtime:

1. Sleep until the reed switch changes state.
2. Wake up and record the current physical reed state.
3. Debounce and normalize the state to magnet presence.
4. If there is no pending unsent value and the current value matches the last value successfully confirmed by the server, skip WiFi and go back to sleep.
5. Connect to the configured WiFi network.
6. Before posting, re-check the reed state. If it changed while connecting, report the newer state.
7. POST JSON to `targetURL`.
8. Record the value as successfully reported only after the server returns a successful HTTP response.
9. After reporting, re-check the reed state again.
10. If the state changed after reporting, report the updated state.
11. Repeat the report-and-recheck cycle until the latest reported state matches the current confirmed reed state.
12. Immediately before sleep, read the current physical reed state one final time.
13. Configure wake for the inverse of that final physical state.
14. Sleep.

Configuration mode:

1. On boot, the ESP32 reads the config button pin.
2. If the config button is down, the ESP32 enters config mode immediately. No hold timer is required.
3. A newly flashed device also enters config mode automatically if required WiFi/target settings are missing.
4. Normal reed reporting is ignored while in config mode.
5. The ESP32 does not go to sleep while in config mode.
6. The onboard LED blinks 3 times, waits 2 seconds, then repeats.
7. The ESP32 starts a password-free WiFi access point named `GMC Toggle`.
8. A captive portal opens a device configuration page.
9. The configuration page is a clean mobile-friendly interface for iPhone-sized screens and stores:
   - `deviceName`: max 20 characters, letters, numbers, and underscores only.
   - `wifiSSID`: normal WiFi SSID used outside config mode.
   - `wifiPass`: normal WiFi password.
   - `targetURL`: local web server endpoint, for example `http://192.168.0.78/trigger.php`.
   - `normallyClosed`: whether the installed reed switch is NC instead of NO. Newly flashed devices default to NO.
10. On save, settings are written to flash and the ESP32 reboots with those settings in place.

Failure behavior:

- If WiFi or HTTP reporting fails, the device stores the latest unsent magnet value.
- It then sleeps using the final physical reed state for GPIO wake, and also arms a timer retry.
- On the next wake, it tries to send the pending value before bringing the server up to date with the current confirmed value.

## HTTP Folder

The `http` folder contains files intended to be copied to the local LAN web server.

- `http/trigger.php` accepts JSON events and writes the latest state to `http/data/<deviceName>.json`.
- `http/index.php` displays the latest known state as plain text and refreshes automatically.

## JSON Contract

Firmware will POST JSON like this:

```json
{
  "deviceName": "pirates_chest1",
  "deviceType": "toggle",
  "data": {
    "value": 0
  }
}
```

`value` is the normalized magnet presence after input debouncing and NO/NC correction. The server should not need to know whether the physical reed switch is NO or NC.

- `1`: magnet is near the reed switch.
- `0`: magnet is away from the reed switch.

The server decides what those values mean for each device. For example, a chest may treat `0` as open because the lid moved the magnet away from the reed switch.

## Reed Switch Normalization

The wake source is based only on the physical GPIO level at the moment the ESP32 goes to sleep. After waking, the firmware will normalize the physical reed reading using the configured switch type before sending status to the server.

- NO switch: closed means magnet near.
- NC switch: open means magnet near.
- `normallyClosed` is the device-side correction setting. It defaults to `false`.

This lets each device correct for NO or NC installations locally while the server always receives a consistent magnet presence value.

## Recommended Input Hardware

For reliable reed readings, wire the reed switch between the reed GPIO and GND, with the GPIO pulled up.

Best compact setup:

- Reed switch: one wire to GPIO3, other wire to GND.
- Reed pullup resistor, 100 kOhm: one side to GPIO3, other side to 3.3 V.
- Reed debounce/noise capacitor, 10 nF ceramic: one side to GPIO3, other side to GND, close to the ESP32 board.
- Config button: one wire to GPIO4, other wire to GND.
- Config button pullup resistor, 100 kOhm: one side to GPIO4, other side to 3.3 V.
- Config button debounce/noise capacitor, 10 nF ceramic: one side to GPIO4, other side to GND, close to the ESP32 board.
- Supply decoupling capacitor, 100 nF ceramic: one side to 3.3 V, other side to GND, close to the ESP32 board.

The firmware should still debounce in software after waking. Hardware filtering is there to reduce false wakes and noisy edges, not to replace firmware confirmation.

For each sleep cycle, the firmware must configure wake for the inverse of the final physical GPIO level read immediately before entering sleep. The `normallyClosed` correction only affects the reported `data.value`, not the wake edge selection.

The firmware expects active-low inputs, meaning the reed switch and config button connect their GPIO to GND when active. The external pullups, debounce capacitors, and 100 nF supply capacitor improve wake reliability and noise resistance. If the input components are omitted and only the ESP32 internal pullups are used, the firmware logic stays the same, but deep-sleep wake reliability depends more heavily on the board and wiring.

## Hardware Pinout

This project uses the following fixed pin assignments for the ESP32-mini-C3 build:

- GPIO3: reed switch input.
- GPIO4: config button input.
- Both inputs are wired to GND when active and pulled up to 3.3 V.

These pins are chosen because common ESP32-C3 mini layouts place GPIO0-GPIO4 on the same side as GND, while GPIO5 is on the opposite side. Project defaults live in [include/project_config.h](include/project_config.h).
