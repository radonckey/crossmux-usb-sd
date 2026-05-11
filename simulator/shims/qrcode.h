#pragma once
// Stub for ricmoo/QRCode. Real QR generation is out of scope for the simulator; we
// only need the header shape so that calling code links. The stub draws nothing.

#include <cstdint>

typedef struct QRCode {
  uint8_t version;
  uint8_t size;
  uint8_t errorCorrection;
  uint8_t* modules;
} QRCode;

inline int qrcode_initText(QRCode*, uint8_t*, uint8_t, uint8_t, const char*) { return 0; }
inline bool qrcode_getModule(QRCode*, uint8_t, uint8_t) { return false; }
