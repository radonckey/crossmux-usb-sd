// HAL GPIO backend for the WebAssembly (Emscripten) simulator.
//
// Same edge-triggered button state machine as HalGPIO_native.cpp, but events arrive
// from JavaScript (keyboard + on-screen touch buttons) via the exported
// simulator_inject_button() instead of from an SDL event pump. millis() (arduino-host)
// replaces SDL_GetTicks() for hold-time tracking.

#include <Arduino.h>  // millis()
#include <HalGPIO.h>

#include <cstdlib>
#include <mutex>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

namespace {
constexpr size_t kButtonCount = 7;

// Matches hardware InputManager edge semantics: events are valid for exactly one
// update() cycle. injectButton() (browser main thread) writes pending* fields;
// update() (firmware thread, once per loop) latches pending → frame and clears
// pending; reads return frame* without consuming.
struct ButtonStates {
  bool pressed = false;
  bool pendingPressed = false;
  bool pendingReleased = false;
  bool framePressed = false;
  bool frameReleased = false;
  unsigned long pressedAtMs = 0;
};

std::mutex& state_mutex() {
  static std::mutex m;
  return m;
}

ButtonStates g_buttons[kButtonCount];
}  // namespace

namespace simulator {
// Called from the browser (via simulator_inject_button) on each key/touch event.
void injectButton(uint8_t buttonIndex, bool down) {
  if (buttonIndex >= kButtonCount) return;
  std::lock_guard<std::mutex> lock(state_mutex());
  ButtonStates& b = g_buttons[buttonIndex];
  if (down && !b.pressed) {
    b.pressed = true;
    b.pendingPressed = true;
    b.pressedAtMs = millis();
  } else if (!down && b.pressed) {
    b.pressed = false;
    b.pendingReleased = true;
  }
}
}  // namespace simulator

void HalGPIO::begin() {}

void HalGPIO::update() {
  std::lock_guard<std::mutex> lock(state_mutex());
  for (auto& b : g_buttons) {
    b.framePressed = b.pendingPressed;
    b.frameReleased = b.pendingReleased;
    b.pendingPressed = false;
    b.pendingReleased = false;
  }
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
  if (buttonIndex >= kButtonCount) return false;
  std::lock_guard<std::mutex> lock(state_mutex());
  return g_buttons[buttonIndex].pressed;
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
  if (buttonIndex >= kButtonCount) return false;
  std::lock_guard<std::mutex> lock(state_mutex());
  return g_buttons[buttonIndex].framePressed;
}

bool HalGPIO::wasAnyPressed() const {
  std::lock_guard<std::mutex> lock(state_mutex());
  for (auto& b : g_buttons) {
    if (b.framePressed) return true;
  }
  return false;
}

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  if (buttonIndex >= kButtonCount) return false;
  std::lock_guard<std::mutex> lock(state_mutex());
  return g_buttons[buttonIndex].frameReleased;
}

bool HalGPIO::wasAnyReleased() const {
  std::lock_guard<std::mutex> lock(state_mutex());
  for (auto& b : g_buttons) {
    if (b.frameReleased) return true;
  }
  return false;
}

unsigned long HalGPIO::getHeldTime() const {
  std::lock_guard<std::mutex> lock(state_mutex());
  unsigned long now = millis();
  unsigned long maxHeld = 0;
  for (auto& b : g_buttons) {
    if (b.pressed && b.pressedAtMs > 0) {
      unsigned long held = now - b.pressedAtMs;
      if (held > maxHeld) maxHeld = held;
    }
  }
  return maxHeld;
}

unsigned long HalGPIO::getPowerButtonHeldTime() const { return 0; }

// No real device to sleep. In the browser we keep the runtime alive; treat deep sleep
// as a no-op (the page stays on the last rendered frame).
void HalGPIO::startDeepSleep() {}

void HalGPIO::verifyPowerButtonWakeup(uint16_t /*requiredDurationMs*/, bool /*shortPressAllowed*/) {}

bool HalGPIO::isUsbConnected() const { return true; }
bool HalGPIO::wasUsbStateChanged() const { return false; }

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const { return WakeupReason::Other; }

HalGPIO gpio;

// ---------------------------------------------------------------------------
// C export consumed by the browser side. buttonIndex matches HalGPIO::BTN_*:
// 0=Back 1=Confirm 2=Left 3=Right 4=Up 5=Down 6=Power. down=1 press, 0 release.
// ---------------------------------------------------------------------------
extern "C" EMSCRIPTEN_KEEPALIVE void simulator_inject_button(int buttonIndex, int down) {
  simulator::injectButton(static_cast<uint8_t>(buttonIndex), down != 0);
}
