# OledAutoSleep

ESP32 motion-activated power manager for an LG WebOS (OLED) TV. A presence sensor on the
ESP32 watches the room; when nobody is around it turns the TV screen off (screensaver)
and eventually powers the TV off. Motion turns the screen back on. A small TFT display
shows the current state and a countdown.

Commands are sent over HTTP/HTTPS to [`webosapitool`](https://github.com/nekorro/webosapitool),
which bridges them to the TV's WebOS API.

> Based on [davidz-yt/lg-oled-auto-sleep](https://github.com/davidz-yt/lg-oled-auto-sleep).
> This variant talks to a standalone `webosapitool` backend (with token auth over
> HTTP/HTTPS) instead of Home Assistant. See [Credits](#credits).

## Hardware

- **LilyGO TTGO T-Display ESP32** — ESP32 board with a built-in 1.14" 135×240 ST7789
  TFT (driven by [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI))
- **LD2410C** mmWave presence sensor (a PIR sensor works too) on GPIO 13
- Powered from the TV's USB port, so cutting TV power also de-powers the ESP32

## Wiring

The presence sensor uses a simple three-wire connection to the T-Display:

| Sensor (LD2410C) | TTGO T-Display |
| ---------------- | -------------- |
| VCC              | 5V             |
| GND              | GND            |
| OUT              | GPIO 13 (`motionPin1`, configurable) |

The TFT display is built into the T-Display board, so it needs no extra wiring.
GPIO 15 (`motionPin2`) is reserved for an optional second sensor but is unused.

## State machine

| State            | Meaning                          |
| ---------------- | -------------------------------- |
| `INIT`           | Initialization                   |
| `MOTION_LCD_ON`  | Motion detected (ESP LCD on)     |
| `MOTION_LCD_OFF` | Motion detected (ESP LCD off)    |
| `NO_MOTION`      | No motion (counting to sleep)    |
| `SCREENSAVER`    | Screensaver on (TV screen off)   |
| `POWER_OFF`      | Power off (TV shutting down)     |

Timings (`sleepDelay`, `powerDelay`, `espLCDSleep`) are constants near the top of
[`OledAutoSleep.ino`](OledAutoSleep.ino).

## Configuration

Credentials live in `config_secrets.h`, which is **gitignored**. Copy the template
and fill in your values before flashing:

```sh
cp config_secrets.example.h config_secrets.h
# then edit config_secrets.h
```

| Macro                     | Description                                               |
| ------------------------- | -------------------------------------------------------- |
| `WIFI_SSID` / `WIFI_PASSWORD` | Your WiFi credentials                                |
| `WEBOS_CONTROLLER_URL`    | Base URL of your `webosapitool` instance (HTTP or HTTPS) |
| `WEBOS_CONTROLLER_DEVICE` | Entity name configured in `webosapitool`                 |
| `WEBOS_CONTROLLER_TOKEN`  | Auth UUID that matches the entity's `auth` in the backend |
| `OTA_PASSWORD`            | Password for over-the-air updates                        |

The TV requests are sent as `POST {url}{device}/{action}` with an
`Authorization: Bearer <token>` header. The scheme is auto-detected from
`WEBOS_CONTROLLER_URL`: `https://` uses TLS, `http://` is plain.

> HTTPS uses `client.setInsecure()` — traffic is encrypted but the server certificate
> is not validated. Fine for a trusted home network; pin a root CA if you need
> protection against man-in-the-middle. Over plain `http://`, the `Authorization`
> token travels in cleartext — only use it on a trusted LAN.

## Flashing

1. **Install the ESP32 core** in the Arduino IDE (Boards Manager → "esp32" by Espressif).
2. **Install [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)** (Library Manager) and select
   the T-Display setup: in `TFT_eSPI/User_Setup_Select.h`, comment out the default and
   uncomment `#include <User_Setups/Setup25_TTGO_T_Display.h>`.
3. **Create your secrets file:** `cp config_secrets.example.h config_secrets.h` and edit it.
4. **Select the board:** *ESP32 Dev Module* (or *LilyGo T-Display* if your core lists it),
   with PSRAM disabled and the default partition scheme.
5. **Connect via USB-C** and upload. On boot the TFT shows progress and the device's IP.

After the first USB flash, subsequent uploads can go over WiFi (see OTA below).

## OTA updates

After the first flash over USB, subsequent uploads can go over the network via
ArduinoOTA (hostname `oledautosleep-esp32`).

## Dependencies

- `TFT_eSPI`, `WiFi`, `HTTPClient`, `WiFiClientSecure`, `ESPmDNS`, `ArduinoOTA`
- A bundled `Button2` library (see `Button2.h` / `Button2.cpp`)

## Credits

Based on [davidz-yt/lg-oled-auto-sleep](https://github.com/davidz-yt/lg-oled-auto-sleep)
by David Z, which pairs a LilyGO TTGO T-Display and an LD2410C presence sensor to
auto-sleep an LG OLED TV via Home Assistant. This fork swaps the backend for a
standalone [`webosapitool`](https://github.com/nekorro/webosapitool) service with
token authentication over HTTP/HTTPS.
