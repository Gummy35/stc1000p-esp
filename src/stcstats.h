#ifndef __STCSTATS_H_
#define __STCSTATS_H_

#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>


#include "linkedlist.h"

class StcData {
  public:
    unsigned long timestamp;
    double temperature;
    double setpoint;
    bool cooling;
    bool heating;
    void assign(unsigned long _timestamp, float _temperature, float _setpoint, bool _cooling, bool _heating);
    StcData();
};

extern LinkedList<StcData*> stc_history;

extern String stcstats_sheetid; //--> spreadsheet script ID
extern String stcstats_sheetname;

//class StcStats {
//  public:
//    
//    
//    void loadHistory();
//    void saveHistory();
void stcstats_loadSettings();
void stcstats_saveSettings();
void stcstats_saveProfiles(String data);
String stcstats_getProfiles();
bool stcstats_applyStcSettings(String data);
String stcstats_getStcSettings();
String getCurrentMetricsJson();
String getHistoryJson();
void probeData();
//  private:
void addPtToHist(unsigned long timestamp, float temperature, float setpoint, bool heating, bool cooling);
//    void sendData(float temperature, float setpoint, bool cooling, bool heating);
//};
//
//extern StcStats STCSTATS;

void stcstats_begin();
void stcstats_loop();
void printmem();

#endif
