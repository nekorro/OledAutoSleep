# OledAutoSleep

ESP32 motion-activated power manager for an LG WebOS (OLED) TV. A PIR sensor on the
ESP32 watches the room; when nobody is around it turns the TV screen off (screensaver)
and eventually powers the TV off. Motion turns the screen back on. A small TFT display
shows the current state and a countdown.

Commands are sent over HTTPS to [`webosapitool`](https://github.com/nekorro/webosapitool),
which bridges them to the TV's WebOS API.

## Hardware

- ESP32 board with a TFT display (configured via [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI))
- PIR (or mmWave) motion sensor on `motionPin1` (GPIO 13)
- Powered from the TV's USB port, so cutting TV power also de-powers the ESP32

## State machine

| State | Meaning                          |
| ----- | -------------------------------- |
| 0     | Initialization                   |
| 1     | Motion detected (ESP LCD on)     |
| 2     | Motion detected (ESP LCD off)    |
| 3     | No motion (counting to sleep)    |
| 4     | Screensaver on (TV screen off)   |
| 5     | Power off (TV shutting down)     |

Timings (`sleepDelay`, `powerDelay`, `espLCDSleep`) are constants near the top of
[`OledAutoSleep.ino`](OledAutoSleep.ino).

## Configuration

Before flashing, fill in the placeholders at the top of `OledAutoSleep.ino`:

| Constant                | Description                                              |
| ----------------------- | ------------------------------------------------------- |
| `ssid` / `password`     | Your WiFi credentials                                   |
| `webosControllerUrl`    | Base URL of your `webosapitool` instance (HTTPS)        |
| `webosControllerDevice` | Entity name configured in `webosapitool`                |
| `webosControllerToken`  | Auth UUID that matches the entity's `auth` in the backend |
| OTA password            | `ArduinoOTA.setPassword(...)` in `setup()`              |

The TV requests are sent as `POST {url}{device}/{action}` with an
`Authorization: Bearer <token>` header over TLS.

> TLS uses `client.setInsecure()` — traffic is encrypted but the server certificate
> is not validated. Fine for a trusted home network; pin a root CA if you need
> protection against man-in-the-middle.

## OTA updates

After the first flash over USB, subsequent uploads can go over the network via
ArduinoOTA (hostname `oledautosleep-esp32`).

## Dependencies

- `TFT_eSPI`, `WiFi`, `HTTPClient`, `WiFiClientSecure`, `ESPmDNS`, `ArduinoOTA`
- A bundled `Button2` library (see `Button2.h` / `Button2.cpp`)
