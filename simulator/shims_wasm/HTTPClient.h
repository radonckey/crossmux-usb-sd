#pragma once
// Curl-free HTTPClient/WiFiClient stub for the WebAssembly simulator build.
//
// The native simulator's shims/HTTPClient.h is libcurl-backed so WeRead/OPDS can hit
// the real network from the host. The browser demo is offline-only (WeReadClient.cpp
// is excluded from the WASM source set; networked activities are stubbed), so this
// stub drops the libcurl dependency entirely. Any call site that still includes this
// header (transitively) compiles and fails gracefully at runtime (GET/POST return an
// error, getString() is empty).
//
// The public surface mirrors shims/HTTPClient.h exactly so it can shadow it on the
// WASM include path (see simulator/CMakeLists.txt EMSCRIPTEN branch).

#include <Stream.h>
#include <WString.h>

#include <cstddef>
#include <cstdint>
#include <string>

enum HTTPC_FOLLOW_REDIRECTS_T {
  HTTPC_DISABLE_FOLLOW_REDIRECTS = 0,
  HTTPC_STRICT_FOLLOW_REDIRECTS,
  HTTPC_FORCE_FOLLOW_REDIRECTS,
};

#ifndef HTTP_CODE_OK
#define HTTP_CODE_OK 200
#endif

class WiFiClient {
 public:
  virtual ~WiFiClient() = default;
  bool connect(const char*, uint16_t) { return false; }
  void stop() {}
  bool connected() { return false; }
  int available() { return 0; }
  int read() { return -1; }
  size_t write(const uint8_t*, size_t) { return 0; }
};

using NetworkClient = WiFiClient;

class HTTPClient {
 public:
  HTTPClient() = default;
  ~HTTPClient() = default;

  HTTPClient(const HTTPClient&) = delete;
  HTTPClient& operator=(const HTTPClient&) = delete;

  bool begin(const String&) { return false; }
  bool begin(WiFiClient&, const String&) { return false; }
  void end() {}

  void setTimeout(uint16_t) {}
  void setFollowRedirects(int) {}
  void setAuthorization(const char*, const char*) {}
  void addHeader(const String&, const String&) {}

  // No network in the browser demo: report a connection failure (negative code, like
  // the Arduino HTTPClient does for HTTPC_ERROR_*).
  int GET() { return -1; }
  int POST(const String&) { return -1; }
  int POST(const uint8_t*, size_t) { return -1; }

  String getString() { return String(""); }
  Stream& getStream() { return emptyStream_; }
  Stream* getStreamPtr() { return &emptyStream_; }
  int getSize() { return 0; }

 private:
  class EmptyStream : public Stream {
   public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    size_t write(uint8_t) override { return 0; }
    void flush() override {}
  };
  EmptyStream emptyStream_;
};
