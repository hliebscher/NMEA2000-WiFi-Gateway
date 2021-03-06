#include "N2KPreferences.h"

N2KPreferences::N2KPreferences() {
}

N2KPreferences::~N2KPreferences() {
    end();
}

void N2KPreferences::registerCallback(std::function<void ()> callback) {
    this->callback = callback;
}

void N2KPreferences::executeCallbacks() {
    if (callback) {
        callback();
    }
}

void N2KPreferences::begin() {

    prefs.begin(PREF_SPACE);
    init();
}

void N2KPreferences::freeEntries() {

    Serial.print("[PREF] Free Entries: "); Serial.println(prefs.freeEntries());
}


void N2KPreferences::end() {
    prefs.end();
}

void N2KPreferences::init() {

    //Retrieve settings, use default if missing and store
    //directly to ensure that everything fits in NVS.
    setBlEnabled(prefs.getBool(PREF_BLUETOOTH_ENABLED,false));
    setBlGPSEnabled(prefs.getBool(PREF_NMEA_BL_GPS_ENABLED,false));
    setDemoEnabled(prefs.getBool(PREF_DEMO_ENABLED, false));
    setUdpBroadcastPort(prefs.getUInt(PREF_WIFI_UDP_PORT, 9876));
    setStationEnabled(prefs.getBool(PREF_WIFI_STA_ENABLED, false));
    setStationHostname(prefs.getString(PREF_WIFI_STA_HOSTNAME));
    setStationSSID(prefs.getString(PREF_WIFI_STA_SSID));
    setStationPassword(prefs.getString(PREF_WIFI_STA_PASSWORD));
    setApSSID(prefs.getString(PREF_WIFI_AP_SSID, WIFI_AP_DEFAULT_SSID));
    setApPassword(prefs.getString(PREF_WIFI_AP_PASSWORD, WIFI_AP_DEFAULT_PASSWORD));
    setNmeaToSerial(prefs.getBool(PREF_NMEA_TO_SERIAL,false));
    setNmeaToSocket(prefs.getBool(PREF_NMEA_TO_SOCKET, false));
    setNmeaToBluetooth(prefs.getBool(PREF_NMEA_TO_BL,true));
    setNmeaToUDP(prefs.getBool(PREF_NMEA_TO_UDP,true));
    setNmea2000ToSerial(prefs.getBool(PREF_NMEA2000_TO_SERIAL,false));
    setNmeaMode((tNMEA2000::tN2kMode) prefs.getUChar(PREF_NMEA2000_MODE, tNMEA2000::N2km_ListenOnly));
}

void N2KPreferences::reset() {
    prefs.clear();
    ESP.restart();
}

void N2KPreferences::factoryReset() {
    nvs_flash_erase();
    ESP.restart();
}

void N2KPreferences::setBlEnabled(bool value) {
    prefs.putBool(PREF_BLUETOOTH_ENABLED,value);
    this->blEnabled = value;
}

void N2KPreferences::setDemoEnabled(bool value) {
    prefs.putBool(PREF_DEMO_ENABLED,value);
    this->demoEnabled = value;
 }

bool N2KPreferences::isBlEnabled() {
    return blEnabled;
}

bool N2KPreferences::isDemoEnabled() {
    return demoEnabled;
}

//WIFI General
void N2KPreferences::setUdpBroadcastPort(int port) {
    prefs.putUInt(PREF_WIFI_UDP_PORT, port);
    this->udpPort = port;
}
int N2KPreferences::getUdpBroadcastPort() {
    return udpPort;
}

//WIFI Station preferences
void N2KPreferences::setStationEnabled(bool staEnabled) {
    prefs.putBool(PREF_WIFI_STA_ENABLED, staEnabled);
    this->staEnabled = staEnabled;
}
bool N2KPreferences::getStationEnabled() {
    return staEnabled;
}
void N2KPreferences::setStationHostname(const String stationHostname) {
    prefs.putString(PREF_WIFI_STA_HOSTNAME, stationHostname);
    this->stationHostname = stationHostname;
}
const char * N2KPreferences::getStationHostname() {
    return stationHostname.c_str();
}
void N2KPreferences::setStationSSID(const String staSSID) {
    prefs.putString(PREF_WIFI_STA_SSID, staSSID);
    this->staSSID = staSSID;
}
const char * N2KPreferences::getStationSSID() {
    return staSSID.c_str();
}
void N2KPreferences::setStationPassword(const String staPassword) {
    prefs.putString(PREF_WIFI_STA_PASSWORD, staPassword);
    this->staPassword = staPassword;
}
const char * N2KPreferences::getStationPassword() {
    return staPassword.c_str();
}

//WIFI AP preferences
void N2KPreferences::setApSSID(const String apSSID) {
    prefs.putString(PREF_WIFI_AP_SSID, apSSID);
    this->apSSID = apSSID;
}
const char * N2KPreferences::N2KPreferences::getApSSID() {
    return apSSID.c_str();
}
void N2KPreferences::setApPassword(const String apPassword) {
    prefs.putString(PREF_WIFI_AP_PASSWORD, apPassword);
    this->apPassword = apPassword;
}
const char * N2KPreferences::getApPassword() {
    return apPassword.c_str();
}

//NMEA Preferens
void N2KPreferences::setNmeaToSerial(bool value) {
    prefs.putBool(PREF_NMEA_TO_SERIAL, value);
    nmeaToSerial = value;
}
bool N2KPreferences::isNmeaToSerial() {
    return nmeaToSerial;
}
void N2KPreferences::setNmeaToSocket(bool value) {
    prefs.putBool(PREF_NMEA_TO_SOCKET, value);
    nmeaToWebSocket = value;
}
bool N2KPreferences::isNmeaToSocket() {
    return nmeaToWebSocket;
}
void N2KPreferences::setNmeaToBluetooth(bool value) {
    prefs.putBool(PREF_NMEA_TO_BL, value);
    nmeaToBluetooth = value;
}
bool N2KPreferences::isNmeaToBluetooth() {
    return nmeaToBluetooth;
}
void N2KPreferences::setNmeaToUDP(bool value) {
    prefs.putBool(PREF_NMEA_TO_UDP, value);
    nmeaToUdp = value;
}
bool N2KPreferences::isNmeaToUDP() {
    return nmeaToUdp;
}
void N2KPreferences::setNmea2000ToSerial(bool value) {
    prefs.putBool(PREF_NMEA2000_TO_SERIAL, value);
    nmea2000ToSerial = value;
}
bool N2KPreferences::isNmea2000ToSerial() {
    return nmea2000ToSerial;    
}
void N2KPreferences::setNmeaMode(tNMEA2000::tN2kMode value) {
    prefs.putUChar(PREF_NMEA2000_MODE, value);
    nmea2000Mode = value;
}
tNMEA2000::tN2kMode N2KPreferences::getNmeaMode() {
    return nmea2000Mode;
}

void N2KPreferences::setBlGPSEnabled(bool value) {
    prefs.putBool(PREF_NMEA_BL_GPS_ENABLED, value);
    nmeaBlGps = value;
}

bool N2KPreferences::isBlGPSEnabled() {
    return nmeaBlGps;
}