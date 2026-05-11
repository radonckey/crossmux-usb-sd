#pragma once

// Shim: only included by lib/hal/HalStorage.cpp, which we replace entirely with
// simulator/hal_native/HalStorage_native.cpp. Header is reserved here so any header
// path that resolves to <SDCardManager.h> doesn't fail.

class SDCardManager {
 public:
  SDCardManager() = default;
};
