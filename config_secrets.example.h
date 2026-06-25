#ifndef CONFIG_SECRETS_H
#define CONFIG_SECRETS_H

// Copy this file to "config_secrets.h" and fill in your real values.
// config_secrets.h is gitignored so your credentials never get committed.

// WiFi credentials
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Arduino OTA (over-the-air update) password
#define OTA_PASSWORD "YOUR_OTA_PASSWORD"

// webosapitool backend
#define WEBOS_CONTROLLER_URL "https://your-webos-controller.example.com/"
#define WEBOS_CONTROLLER_DEVICE "YOUR_TV_DEVICE_ID"
#define WEBOS_CONTROLLER_TOKEN "YOUR_AUTH_UUID"

#endif // CONFIG_SECRETS_H
