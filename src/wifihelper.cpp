// Use from 0 to 4. Higher number, more debugging messages and memory usage.
//#define _WIFIMGR_LOGLEVEL_ 3
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#else
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <WiFiClient.h>

// From v1.1.0
#include "stcstats.h"

// LittleFS has higher priority than SPIFFS
#ifdef ESP8266
#define ESP_DRD_USE_LITTLEFS true
#define ESP_DRD_USE_SPIFFS false
#define ESP_DRD_USE_EEPROM false

#include "FS.h"

// The library will be depreciated after being merged to future major Arduino esp32 core release 2.x
// At that time, just remove this library inclusion
#include <LITTLEFS.h> // https://github.com/lorol/LITTLEFS

FS *filesystem = &LittleFS;
#define FileFS LittleFS
#define FS_Name "LittleFS"
#define WebServer ESP8266WebServer

#else
#define ESP_DRD_USE_LITTLEFS false
#define ESP_DRD_USE_SPIFFS true
#define ESP_DRD_USE_EEPROM false

#include <SPIFFS.h>
FS *filesystem = &SPIFFS;
#define FileFS SPIFFS
#define FS_Name "SPIFFS"

#endif

#define DOUBLERESETDETECTOR_DEBUG true //false

#include <ESP_DoubleResetDetector.h> //https://github.com/khoih-prog/ESP_DoubleResetDetector

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

//DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
DoubleResetDetector *drd;
//////

#include <ArduinoOTA.h>

#define FORMAT_FILESYSTEM false
#define DBG_OUTPUT_PORT Serial

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;

#include <WiFiManager.h>

WiFiManager *wm;

#define HTTP_PORT 80

WebServer server(HTTP_PORT);

// Server handlers

void updateSettings()
{
  stcstats_sheetid = server.arg("sheetid");
  stcstats_sheetname = server.arg("sheetname");
  stcstats_saveSettings();
  String success = "1";

  String json = "{\"sheetid\":\"" + String(stcstats_sheetid) + "\",";
  json += "\"sheetname\":\"" + String(stcstats_sheetname) + "\",";
  json += "\"success\":\"" + String(success) + "\"}";

  server.send(200, "application/json", json);
  Serial.println("Google sheet settings updated");
}

void saveProfiles()
{
  String data = server.arg("plain");

  stcstats_saveProfiles(data);

  String success = "1";
  String json = "{\"success\":\"1\"}";

  server.send(200, "application/json", json);
  Serial.println("STC profiles updated");
}

void getProfiles()
{
  String data = stcstats_getProfiles();
  server.send(200, "application/json", data);
  Serial.print("Send profiles");
}

void applyStcSettings()
{
  String data = server.arg("plain");

  stcstats_applyStcSettings(data);

  String success = "1";
  String json = "{\"success\":\"1\"}";

  server.send(200, "application/json", json);
  Serial.println("STC settings updated");
}

void getStcSettings()
{
  String data = stcstats_getStcSettings();
  server.send(200, "application/json", data);
  Serial.print("Send stc settings");
}

void sendMesures()
{
  String json = getCurrentMetricsJson();
  server.send(200, "application/json", json);
  Serial.println("Send measures");
}

void sendTabMesures()
{
  StcData *base = new StcData();
  StcData *latest = base;
  if (stc_history.size() > 0)
  {
    latest = stc_history.get(stc_history.size() - 1);
    base = latest;
  }
  if (stc_history.size() > 1)
  {
    base = stc_history.get(stc_history.size() - 2);
  }

  double temp = base->temperature;
  String json = "[";
  json += "{\"serie\":\"Temperature\",\"value\":\"" + String(latest->temperature) + "\",\"unit\":\"°C\",\"glyph\":\"glyphicon-indent-left\",\"previous\":\"" + String(temp) + "\"},";
  temp = base->setpoint;
  json += "{\"serie\":\"Setpoint\",\"value\":\"" + String(latest->setpoint) + "\",\"unit\":\"°C\",\"glyph\":\"glyphicon-tint\",\"previous\":\"" + String(temp) + "\"}";
  json += "]";
  server.send(200, "application/json", json);
  Serial.println("Send data tab");
}

void sendHistory()
{
  String json = getHistoryJson();
  server.send(200, "application/json", json); // Envoi l'historique au client Web - Send history data to the web client
  Serial.println(json);
  Serial.println("Send History");
}

void server_start()
{
  server.on("/tabmesures.json", sendTabMesures);
  server.on("/mesures.json", sendMesures);
  server.on("/graph_temp.json", sendHistory);
  server.on("/savesettings", updateSettings);
  server.on("/saveprofiles", saveProfiles);
  server.on("/getprofiles", getProfiles);
  server.on("/applystcsettings", applyStcSettings);
  server.on("/getstcsettings", getStcSettings);

  server.serveStatic("/js", FileFS, "/js");
  server.serveStatic("/css", FileFS, "/css");
  server.serveStatic("/img", FileFS, "/img");
  server.serveStatic("/", FileFS, "/index.html");

  server.begin();
  Serial.println("HTTP server started");
}

void wifi_start()
{
  if (!FileFS.begin())
  {
    Serial.println("Mount failed");
    FileFS.format();
  }
  else
  {
    Serial.println("Mount succesfull");
    // STCSTATS.loadHistory();
  }
  delay(50);

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // put your setup code here, to run once:
  Serial.begin(115200);

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset())
  {
    Serial.println(F("DRD"));
    initialConfig = true;
  }
  wm = new WiFiManager();
  if (wm->getWiFiSSID() == "")
  {
    Serial.println(F("No AP credentials"));
    initialConfig = true;
  }
  if (initialConfig)
  {
    Serial.println(F("Starting Config Portal"));
    if (!wm->startConfigPortal())
    {
      Serial.println(F("Not connected to WiFi"));
    }
    else
    {
      Serial.println(F("connected"));
    }
  }
  else
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin();
  }
  unsigned long startedAt = millis();
  Serial.print(F("After waiting "));
  int connRes = WiFi.waitForConnectResult();
  float waited = (millis() - startedAt);
  Serial.print(waited / 1000);
  Serial.print(F(" secs , Connection result is "));
  Serial.println(connRes);
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(F("Failed to connect"));
    ESP.restart();
  }
  else
  {
    Serial.print(F("Local IP: "));
    Serial.println(WiFi.localIP());
  }

  ArduinoOTA.begin();
  server_start();
}

void wifi_loop()
{
  ArduinoOTA.handle();
  drd->loop();
  server.handleClient();
}