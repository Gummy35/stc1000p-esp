// #ifdef ESP8266
// #include <LittleFS.h>
// #define _FS LittleFS
// #else
// #include <LITTLEFS.h>
// #define _FS LITTLEFS
// #endif

#ifdef ESP8266
#include <LittleFS.h>
#define _FS LittleFS
#else
#define _FS SPIFFS
#include <SPIFFS.h>
#endif

//#include <TimeLib.h>
//#include <NtpClientLib.h>
#include "stcstats.h"
//#include "mqtt.h"
#include <ArduinoJson.h>
#include "Stc1000p.h"
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

const PROGMEM char *ntpServer = "europe.pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

#define STCSTATS_SETTINGS_FILE "/stcstats.json"
#define STCSTATS_PROFILES_FILE "/profiles.json"

String freeHeap;
int sizeHist = 24;                           // Taille historique (7h x 12pts) - History size
unsigned long intervalHist = 1000 * 10;      //* 60 * 60 * 5;  // 5 mesures / heure - 5 measures / hours
unsigned long previousMillis = intervalHist; // Dernier point enregistré dans l'historique - time of last point added
LinkedList<StcData *> stc_history = LinkedList<StcData *>();

String stcstats_sheetid = ""; //--> spreadsheet script ID
String stcstats_sheetname = "";

#ifdef ESP32
Stc1000p stc1000p(16, INPUT_PULLDOWN);
#else
Stc1000p stc1000p(D0, INPUT_PULLDOWN_16);
#endif

unsigned long now() {
  return timeClient.getEpochTime();
}

void StcData::assign(unsigned long _timestamp, float _temperature, float _setpoint, bool _cooling, bool _heating)
{
  timestamp = _timestamp;
  temperature = _temperature;
  setpoint = _setpoint;
  cooling = _cooling;
  heating = _heating;
}

StcData::StcData()
{
  timestamp = now();
  temperature = 0.0F;
  setpoint = 0.0F;
  cooling = false;
  heating = false;
}

const long probeInterval = 10000;

void printmem()
{
  long fh = ESP.getFreeHeap();
  char fhc[20];

  ltoa(fh, fhc, 10);
  freeHeap = String(fhc);

  Serial.println("Heap " + freeHeap);
}

String getCurrentMetricsJson()
{
  StcData *d;
  if (stc_history.size() > 0)
  {
    d = stc_history.get(stc_history.size() - 1);
  }
  String json = "{\"t\":\"" + String(d->temperature) + "\",";
  json += "\"sp\":\"" + String(d->setpoint) + "\"}";
  return json;
}

String getHistoryJson()
{
  String json;
  StaticJsonBuffer<1000> jsonBuffer; // Buffer static contenant le JSON courant - Current JSON static buffer
  JsonObject &jsonData = jsonBuffer.createObject();
  JsonArray &timestamp = jsonData.createNestedArray("timestamp");
  JsonArray &hist_t = jsonData.createNestedArray("t");
  JsonArray &hist_sp = jsonData.createNestedArray("sp");

  StcData *d;
  for (int i = 0; i < stc_history.size(); i++)
  {
    StcData *d = stc_history.get(i);
    timestamp.add(d->timestamp);
    hist_t.add(d->temperature);
    hist_sp.add(d->setpoint);
  }

  jsonData.printTo(json); // Export du JSON dans une chaine - Export JSON object as a string
  // Serial.print(json);
  return json;
}

void probeData()
{
  float temp;
  float sp;
  bool cooling;
  bool heating;

  bool res = stc1000p.readTemperature(&temp)
    && stc1000p.readSetpoint(&sp)
    && stc1000p.readHeating(&heating)
    && stc1000p.readCooling(&cooling);
  
  res = true;
  temp = 20.0F + (float)(random(40) - random(40)) / 10.0F;
  sp = 20.0F;
  cooling = temp > (sp + 0.5F);
  heating = temp < (sp - 0.5F);

  addPtToHist(now(), temp, sp, heating, cooling);
}

// Subroutine for sending data to Google Sheets
void sendData(float temperature, float setpoint, bool cooling, bool heating)
{
  if ((stcstats_sheetid.length() == 0) || (stcstats_sheetname.length() == 0))
  {
    return;
  }
  WiFiClientSecure client;

  const char *host = "script.google.com";
  const int httpsPort = 443;

  client.setInsecure();

  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort))
  {
    Serial.println("connection failed");
    return;
  }
  //----------------------------------------

  //----------------------------------------Processing data and sending data
  String string_temperature = String(temperature);
  String string_setpoint = String(setpoint);
  String url = "/macros/s/" + stcstats_sheetid + "/exec?sheet=" + stcstats_sheetname + "&temperature=" + string_temperature + "&setpoint=" + string_setpoint + "&cooling=" + (cooling ? "1" : "0") + "&heating=" + (heating ? "1" : "0");
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               F("User-Agent: ESP\r\n") +
               F("Connection: close\r\n\r\n"));

  // Serial.println("request sent");
  //----------------------------------------

  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (line == "\r")
    {
      //      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  delay(100);
}

void addPtToHist(unsigned long timestamp, float temperature, float setpoint, bool heating, bool cooling)
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > intervalHist)
  {
    previousMillis = currentMillis;
    if (timestamp > 0)
    {
      sendData(temperature, setpoint, cooling, heating);
      StcData *d;
      if (stc_history.size() == sizeHist)
      {
        d = stc_history.rotate();
      }
      else
      {
        d = new StcData();
        stc_history.push(d);
      }
      d->assign(timestamp, temperature, setpoint, cooling, heating);
    }
  }
}

void stcstats_loadSettings()
{
  File file = _FS.open(STCSTATS_SETTINGS_FILE, "r");
  if (!file)
  {
    Serial.println("No settings file");
  }
  else
  {
    size_t size = file.size();
    std::unique_ptr<char[]> buf(new char[size]);
    file.readBytes(buf.get(), size);
    StaticJsonBuffer<256> jsonBuffer;
    JsonObject &tmpData = jsonBuffer.parseObject(buf.get());
    if (!tmpData.success())
    {
      Serial.println(F("Can't read JSON file"));
    }
    else
    {
      stcstats_sheetid = tmpData["sheetid"] | "";
      stcstats_sheetname = tmpData["sheetname"] | "";
      // String spreadsheetId = "AKfycbxJuVv3XHsbBtr3JrG8K5rvpI5CojptM_WhwqYPLe3XtMOsPdkj-xn95A"; //--> spreadsheet script ID
      // String sheet = "Fermenter1";

      Serial.println("sheetid " + stcstats_sheetid);
      Serial.println("sheetname " + stcstats_sheetname);
    }
    file.close();
  }
}

void stcstats_saveSettings()
{
  char json[256];
  File file = _FS.open(STCSTATS_SETTINGS_FILE, "w");
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject &jsonData = jsonBuffer.createObject();
  jsonData["sheetid"] = stcstats_sheetid;
  jsonData["sheetname"] = stcstats_sheetname;
  jsonData.printTo(json, sizeof(json));
  file.print(json);
  file.close();
}

void stcstats_saveProfiles(String data)
{
  char json[1024];
  File file = _FS.open(STCSTATS_PROFILES_FILE, "w");
  file.print(data);
  file.close();
}

String stcstats_getProfiles()
{
  String result;
  File file = _FS.open(STCSTATS_PROFILES_FILE, "r");
  if (!file)
  {
    Serial.println("No profile file");
    bool res = true;
    float sp = 0.0F;
    int dh = 0;
    String sps;
    String dhs;
    result = "[";
    for (int program = 0; program < 6; program++)
    {
      sps = "\"sp\":[";
      dhs = "\"dh\":[";
      for (int step = 0; step < 10; step++)
      {
        res = res && stc1000p.readProfileSetpoint(program, step, &sp);
        sps += String(sp);
        if (step < 9)
        {
          sps += ",";
        }
        else
        {
          sps += "],";
        }
      }
      for (int step = 0; step < 9; step++)
      {
        res = res && stc1000p.readProfileDuration(program, step, &dh);
        dhs += String(dh);
        if (step < 8)
        {
          dhs += ",";
        }
        else
        {
          dhs += "],";
        }
      }
      result += "{" + sps + dhs + "\"name\":\"Profile " + String(program + 1) + "\"}";
      if (program < 5)
      {
        result += ",";
      }
      else
      {
        result += "]";
      }
    }
    if (!res)
    {
      Serial.println("Can't read profile from STC");
      result = F("[{\"sp\":[0,0,0,0,0,0,0,0,0,0],\"dh\":[0,0,0,0,0,0,0,0,0],\"name\":\"Profile 1\"},{\"sp\":[0,0,0,0,0,0,0,0,0,0],\"dh\":[0,0,0,0,0,0,0,0,0],\"name\":\"Profile 2\"},{\"sp\":[0,0,0,0,0,0,0,0,0,0],\"dh\":[0,0,0,0,0,0,0,0,0],\"name\":\"Profile 3\"},{\"sp\":[0,0,0,0,0,0,0,0,0,0],\"dh\":[0,0,0,0,0,0,0,0,0],\"name\":\"Profile 4\"},{\"sp\":[0,0,0,0,0,0,0,0,0,0],\"dh\":[0,0,0,0,0,0,0,0,0],\"name\":\"Profile 5\"},{\"sp\":[0,0,0,0,0,0,0,0,0,0],\"dh\":[0,0,0,0,0,0,0,0,0],\"name\":\"Profile 6\"}]");
    }
  }
  else
  {
    result = file.readString();
    file.close();
  }
  Serial.println(result);
  return result;
}

bool applyStcProfiles()
{
  // Open file.
  if (!_FS.exists(STCSTATS_PROFILES_FILE))
  {
    return false;
  }

  File file = _FS.open(STCSTATS_PROFILES_FILE, "r");
  if (!file)
  {
    Serial.print("Cannot process ");
    Serial.print(STCSTATS_PROFILES_FILE);
    Serial.println(": Failed to open.");
    return false;
  }

  static const uint16_t MAX = 500;
  StaticJsonBuffer<512> jsonBuffer;

  String buffer;
  int bufferLen = 0;
  int val;
  char ch;
  int profilenum = 0;
  bool result = true;

  // read array start
  if ((val = file.read()) == -1)
  {
    return false;
  }
  if (char(val) != '[')
  {
    return false;
  }

  buffer = "";
  bufferLen = 0;

  while ((val = file.read()) != -1)
  {
    ch = char(val);

    // Lookup expansion.
    if (ch == '{')
    {
      // Clear out buffer.
      buffer = "{";

      // Process substitution.
      bool found = false;
      while (!found && (val = file.read()) != -1)
      {
        ch = char(val);
        if (ch == '}')
        {
          found = true;
        }
        buffer += ch;
      }

      // Check for bad exit.
      if (val == -1 && !found)
      {
        Serial.print("Cannot process ");
        Serial.print(STCSTATS_PROFILES_FILE);
        Serial.println(": Unable to parse.");
        return false;
      }

      Serial.println(buffer);

      JsonObject &root = jsonBuffer.parseObject(buffer);
      JsonArray &sps = root["sp"];
      JsonArray &dhs = root["dh"];

      for (int i = 0; i < 10; i++)
      {
        float sp = sps[i].as<float>();
        Serial.print("writeProfileSetpoint " + String(profilenum));
        Serial.print(" " + String(i));
        Serial.println(" "+ String(sp));
        result &= stc1000p.writeProfileSetpoint(profilenum, i, sp);
      }
      for (int i = 0; i < 9; i++)
      {
        int dh = dhs[i].as<int>();
        Serial.print("writeProfileDuration " + String(profilenum));
        Serial.print(" " + String(i));
        Serial.println(" "+ String(dh));
        result &= stc1000p.writeProfileDuration(profilenum, i, dh);
      }
      profilenum++;
    }
  }
  file.close();
  return result;
}

bool stcstats_applyStcSettings(String data)
{
  float sp = 0.0F, hy = 0.5F, tc = 0.0F, sa = 0.0F;
  int st = 0, dh = 0, cd = 2, hd = 2, rn_int = 0;
  bool rp = false, on = true;
  Stc1000pRunMode rn = Stc1000pRunMode::PR0;
  int power_on_addr = 127;

  bool res = applyStcProfiles();
  StaticJsonBuffer<1000> jsonBuffer;
  JsonObject &jsonData = jsonBuffer.parseObject(data);

  sp = jsonData["sp"].as<float>();
  rn_int = jsonData["rn"].as<int>();
  hy = jsonData["hy"].as<float>();
  tc = jsonData["tc"].as<float>();
  sa = jsonData["sa"].as<float>();
  st = jsonData["st"].as<int>();
  dh = jsonData["dh"].as<int>();
  cd = jsonData["cd"].as<int>();
  hd = jsonData["hd"].as<int>();
  rp = jsonData["rp"].as<bool>();
  on = jsonData["on"].as<bool>();
 
  switch(rn_int) {
    case 0:
      rn = Stc1000pRunMode::PR0;
      break;
    case 1:
      rn = Stc1000pRunMode::PR1;
      break;
    case 2:
      rn = Stc1000pRunMode::PR2;
      break;
    case 3:
      rn = Stc1000pRunMode::PR3;
      break;
    case 4:
      rn = Stc1000pRunMode::PR4;
      break;
    case 5:
      rn = Stc1000pRunMode::PR5;
      break;
    case 6:
      rn = Stc1000pRunMode::TH;
      break;
    default:
      res = false;
  }

  res &= stc1000p.writeSetpoint(sp);
  res &= stc1000p.writeRunMode(rn);
  res &= stc1000p.writeHysteresis(hy);
  res &= stc1000p.writeTemperatureCorrection(tc);
  res &= stc1000p.writeSetpointAlarm(sa);
  res &= stc1000p.writeCurrentStep(st);
  res &= stc1000p.writeCurrentDuration(dh);
  res &= stc1000p.writeCoolingDelay(cd);
  res &= stc1000p.writeHeatingDelay(hd);
  res &= stc1000p.writeRamping(rp);
  res &= stc1000p.writeEepromBool(power_on_addr, on);
  return res;
}

String stcstats_getStcSettings()
{
  float sp = 0.0F, hy = 0.5F, tc = 0.0F, sa = 0.0F;
  int st = 0, dh = 0, cd = 2, hd = 2;
  bool rp = false, on = true;
  Stc1000pRunMode rn = Stc1000pRunMode::PR0;

  int power_on_addr = 127;

  bool res = stc1000p.readSetpoint(&sp);
  res &= stc1000p.readRunMode(&rn);
  res &= stc1000p.readHysteresis(&hy);
  res &= stc1000p.readTemperatureCorrection(&tc);
  res &= stc1000p.readSetpointAlarm(&sa);
  res &= stc1000p.readCurrentStep(&st);
  res &= stc1000p.readCurrentDuration(&dh);
  res &= stc1000p.readCoolingDelay(&cd);
  res &= stc1000p.readHeatingDelay(&hd);
  res &= stc1000p.readRamping(&rp);
  res &= stc1000p.readEepromBool(power_on_addr, &on);

  if (!res)
  {
    Serial.println("Could not access STC");
  }

  int data = 0;
  switch (rn)
  {
  case Stc1000pRunMode::PR0:
    data = 0;
    break;
  case Stc1000pRunMode::PR1:
    data = 1;
    break;
  case Stc1000pRunMode::PR2:
    data = 2;
    break;
  case Stc1000pRunMode::PR3:
    data = 3;
    break;
  case Stc1000pRunMode::PR4:
    data = 4;
    break;
  case Stc1000pRunMode::PR5:
    data = 5;
    break;
  case Stc1000pRunMode::TH:
    data = 6;
    break;
  default:
    data = 0;
  }

  String result = "{\"rn\":" + String(data) + ",";
  result += "\"sp\":" + String(sp) + ",";
  result += "\"hy\":" + String(hy) + ",";
  result += "\"tc\":" + String(tc) + ",";
  result += "\"sa\":" + String(sa) + ",";
  result += "\"st\":" + String(st) + ",";
  result += "\"dh\":" + String(dh) + ",";
  result += "\"cd\":" + String(cd) + ",";
  result += "\"hd\":" + String(hd) + ",";
  result += "\"rp\":" + String(rp ? "true" : "false") + ",";
  result += "\"on\":" + String(on ? "true" : "false") + "}";

  Serial.println(result);
  return result;
}

unsigned long lastProbeTime = millis();

void stcstats_begin()
{
  // Serial.print("Uptime :");
  // Serial.println(NTP.getUptime());
  // Serial.print("LastBootTime :");
  // Serial.println(NTP.getLastBootTime());
  // NTP.getTime();

timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(3600);

  // // Serveur NTP, decalage horaire, heure été - NTP Server, time offset, daylight
  // NTP.begin("pool.ntp.org", 1, true);
  // NTP.setInterval(60000);
  // delay(500);


}

void stcstats_loop()
{
  unsigned long currentProbeTime = millis();
  if (currentProbeTime - lastProbeTime > probeInterval)
  {
    timeClient.update();
    lastProbeTime = currentProbeTime;
    probeData();
  }
}
