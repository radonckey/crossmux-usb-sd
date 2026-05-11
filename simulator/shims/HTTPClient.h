#pragma once
// Stub: HTTPClient is out of scope on host. Provides parse-only API shape.

#include <WString.h>
#include <Stream.h>

#include <cstdint>

class HTTPClient {
 public:
  bool begin(const String&) { return false; }
  bool begin(class WiFiClient&, const String&) { return false; }
  void end() {}
  int GET() { return -1; }
  int POST(const String&) { return -1; }
  int POST(const uint8_t*, size_t) { return -1; }
  String getString() { return String(); }
  Stream* getStreamPtr() { return nullptr; }
  void setTimeout(uint16_t) {}
  void addHeader(const String&, const String&) {}
  int getSize() { return 0; }
  void setAuthorization(const char*, const char*) {}
};

class WiFiClient {
 public:
  bool connect(const char*, uint16_t) { return false; }
  void stop() {}
  int available() { return 0; }
  int read() { return -1; }
  size_t write(const uint8_t*, size_t) { return 0; }
};
