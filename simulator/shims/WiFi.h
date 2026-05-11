#pragma once
// Header-only stub: lets WiFi-aware code parse on host. Real connectivity is out of
// scope for arduino-host; consumers that actually need WiFi provide their own backend.

#include <Print.h>
#include <WString.h>

#include <cstdint>

class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
  operator uint32_t() const { return 0; }
  String toString() const { return String("0.0.0.0"); }
};

class WiFiClass {
 public:
  void mode(int) {}
  int begin(const char* = nullptr, const char* = nullptr) { return 0; }
  int status() { return 0; }
  void disconnect(bool = false) {}
  IPAddress localIP() { return IPAddress(); }
  String macAddress() { return String("00:00:00:00:00:00"); }
  String SSID() { return String(); }
  int RSSI() { return 0; }
  int scanNetworks() { return 0; }
  String SSID(int) { return String(); }
  int encryptionType(int) { return 0; }
  int RSSI(int) { return 0; }
};

enum WiFiMode { WIFI_OFF = 0, WIFI_STA, WIFI_AP, WIFI_AP_STA };
enum WiFiStatus { WL_NO_SHIELD = 0, WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_SCAN_COMPLETED, WL_CONNECTED,
                  WL_CONNECT_FAILED, WL_CONNECTION_LOST, WL_DISCONNECTED };

extern WiFiClass WiFi;
