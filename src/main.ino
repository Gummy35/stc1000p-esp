#include "wifihelper.h"
#include <Arduino.h>
//#include <ESP8266WiFi.h>

#ifdef ESP8266
  #include <LittleFS.h>
#else
  #include <SPIFFS.h>
#endif

// #include <TimeLib.h>
#include "stcstats.h"

void setup() {
Serial.begin ( 115200 );
  wifi_start();
  delay(50);
  stcstats_loadSettings();
  stcstats_saveSettings();
}

void loop() {
  // put your main code here, to run repeatedly:
  wifi_loop();
  stcstats_loop();
  delay(200);
}
