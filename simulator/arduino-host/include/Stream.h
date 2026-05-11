#pragma once

#include <Print.h>

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;

  // Read until terminator or timeout; default impl reads byte-by-byte.
  virtual String readStringUntil(char terminator);
};
