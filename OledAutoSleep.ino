#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "time.h"
#include "config_secrets.h" // WiFi / OTA / backend credentials (gitignored)

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const int motionPin1 = 13;
const int motionPin2 = 15; //Not Used
const int screenRotation = 3;

String webosControllerUrl = WEBOS_CONTROLLER_URL;
String webosControllerDevice = WEBOS_CONTROLLER_DEVICE;
String webosControllerToken = WEBOS_CONTROLLER_TOKEN;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 60 * 60; // GMT+3
const int   daylightOffset_sec = 0;

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library   
TFT_eSprite img = TFT_eSprite(&tft);

unsigned long lastMotion; //Keep track of last time motion was detected 
unsigned long firstDetectionTime; //The first timestamp when motion is detected (for the espLCDSleep)
unsigned long sleepDelay = 5000; //Delay before OLED TV screensaver (ms)
unsigned long powerDelay = sleepDelay + 3600000; //Delay before power off OLED TV (ms)
unsigned long espLCDSleep = 15000; //Delay before turning off LCD on ESP sensor when motion is first detected (ms)

// Current stage of the motion/power state machine.
enum State {
  INIT,           // Initialization
  MOTION_LCD_ON,  // Motion detected, ESP LCD on
  MOTION_LCD_OFF, // Motion detected, ESP LCD off
  NO_MOTION,      // No motion, counting down to screensaver
  SCREENSAVER,    // Screensaver on (TV screen off)
  POWER_OFF       // Power off (TV shutting down)
};

// TV command sent to the backend (passed to rest_api_action).
enum TvAction {
  SCREEN_ON,    // turn TV screen on
  SCREEN_OFF,   // turn TV screen off
  TV_POWER_OFF  // power off TV
};

State state = INIT;
volatile int motionState1;
volatile int motionState2;

boolean detectMotion = true; //Motion is detected (PIR or mmWave)

// Sends a TV command to the backend. `action` is a TvAction value.
void rest_api_action(int action)
{
    WiFiClientSecure client;
    client.setInsecure(); // Encrypt traffic but skip TLS certificate validation
    HTTPClient http;
    int httpResponseCode;
    String url;
    String endpoint;

    if(WiFi.status() != WL_CONNECTED){
      Serial.println("WiFi Disconnected");
      tft.fillScreen(TFT_RED);
      img.setTextColor(TFT_BLACK);
      tft.println("ERROR: WiFi Disconnected");
      delay(5000);
      ESP.restart();
    }

    switch (action) {
        case SCREEN_ON:
          Serial.println("Sending Screen ON Request");
          endpoint = "turn_on_screen";
          break;
        case SCREEN_OFF:
          Serial.println("Sending Screen OFF Request");
          endpoint = "turn_off_screen";
          break;
        case TV_POWER_OFF:
          Serial.println("Sending Power Off Request");
          endpoint = "power_off";
          break;
    }

    url = webosControllerUrl + webosControllerDevice + "/" + endpoint;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + webosControllerToken);
    httpResponseCode = http.POST("");

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    http.end();
}

void IRAM_ATTR motionISR1()
{
    motionState1 = digitalRead(motionPin1);
}

void IRAM_ATTR motionISR2()
{
    motionState2 = digitalRead(motionPin2);
}

void setup()
{
    
    Serial.begin(115200);
    Serial.println("Start");
    
    tft.init();
    img.createSprite(240,135);

    tft.setRotation(screenRotation);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.setTextSize(1);

    tft.println("Booting...");

    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    tft.println("Connecting to WiFi");
    unsigned long connectTime = millis(); // millis() is unsigned long; int would truncate/overflow
    while(WiFi.status() != WL_CONNECTED) {
      delay(200);
      tft.print(".");
      if (millis() - connectTime >= 5000) { // On the rare occasion it gets stuck trying to connect, a reboot fixes it.
        ESP.restart();
      }
    }

    tft.println("IP Address: ");
    IPAddress ip = WiFi.localIP();    
    tft.println(ip);

    // Enable Arduino OTA updates
    ArduinoOTA.setHostname("oledautosleep-esp32");
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();    
    tft.println("Arduino OTA Enabled");

    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    tft.println("Setting up Time Server");

    tft.println("Boot up complete!");
    delay(2000);

    pinMode(motionPin1, INPUT_PULLDOWN);  
    pinMode(motionPin2, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(motionPin1), motionISR1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(motionPin2), motionISR2, CHANGE);
    motionISR1(); //Initialize motionState1 value
    motionISR2(); //Initialize motionState2 value
    lastMotion = millis();
}

void loop()
{
    ArduinoOTA.handle();
    // detectMotion = motion_sensor_scan(); // Legacy code for my Polling (vs Interupt) implementation.
    detectMotion = motionState1 == HIGH || motionState2 == HIGH;

    if (detectMotion) {
      lastMotion = millis();
    }

    if (!detectMotion && (millis() - lastMotion >= powerDelay)) {
      Serial.println("Time to power off the TV");
      state = POWER_OFF;
      draw_screen();
      rest_api_action(TV_POWER_OFF);
      delay(10*60*1000); //Wait 10 minutes for power off, if still not off try again. This assumes the ESP is connected to the OLED TV USB power source, such that it cuts power when the TV is Off.

    } else if (!detectMotion && (millis() - lastMotion >= sleepDelay)) {
      if (state != SCREENSAVER) {
        Serial.println("Turning Screensaver On");
        state = SCREENSAVER;
        rest_api_action(SCREEN_OFF);
      }
      draw_screen();

    } else {
      if (detectMotion) {
        if (state != MOTION_LCD_ON && state != MOTION_LCD_OFF) { //Not already in a motion detected state
          Serial.println("Motion Detected");
          state = MOTION_LCD_ON;
          firstDetectionTime = millis();
          rest_api_action(SCREEN_ON);
        }
        if (state == MOTION_LCD_ON && (millis() - firstDetectionTime >= espLCDSleep)) {
          state = MOTION_LCD_OFF;
        }
        draw_screen();

      } else { //No longer detecting motion but it hasn't been long enough to go to Screensaver or Power Off State.
        state = NO_MOTION;
        draw_screen();
      }
    }
}

void draw_screen() {
    
  img.fillRect(0,0,240,135,TFT_BLACK);
  img.setCursor(0, 0, 2);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(2);

  switch (state) {
    case INIT:
      break;
    case MOTION_LCD_ON:
      draw_border(TFT_GREEN);
      img.setTextDatum(MC_DATUM);
      img.drawString("Motion Detected", 240/2, 135/2);
      break;
    case MOTION_LCD_OFF: //LCD Screensaver while actively on OLED TV
      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        img.println("Issue trying to get time..");
        break;
      }
      img.setTextColor(TFT_DARKGREY);
      img.setCursor(15,10);
      img.println(&timeinfo, "%A");
      img.setCursor(15,40);
      img.println(&timeinfo, "%B %d");
      img.setTextSize(3);
      img.setCursor(15,75);
      img.println(&timeinfo, "%H:%M");
      break;
    case NO_MOTION:
      draw_border(TFT_YELLOW);
      img.setCursor(15,10);
      img.println("No Motion");
      img.setCursor(15,40);
      img.println("Screensaver in:");
      img.setTextSize(3);
      img.setCursor(15,75);
      img.println((lastMotion + sleepDelay - millis())/1000 + 1);
      break;
    case SCREENSAVER:
      draw_border(TFT_RED);
      img.setCursor(15,10);
      img.println("Screensaver On");
      img.setCursor(15,40);
      img.println("Shut Down in:");
      img.setCursor(15,75);
      img.setTextSize(3);
      img.println((lastMotion + powerDelay - millis())/1000 + 1);
      break;
    case POWER_OFF:
      draw_border(TFT_VIOLET);
      img.setTextDatum(MC_DATUM);
      img.drawString("Shutting Down TV", 240/2, 135/2);
      break;
  }

  img.pushSprite(0,0);

}

void draw_border(uint32_t color) {
  img.fillRect(0, 0, 240, 4, color);
  img.fillRect(0, 0, 4, 135, color);
  img.fillRect(0, 130, 240, 4, color);
  img.fillRect(235, 0, 4, 135, color);

}
