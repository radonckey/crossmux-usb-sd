// HAL GPIO backend for the desktop simulator.
//
// Maintains the 7 logical button states from HalGPIO::BTN_* and accepts keyboard
// events forwarded by simulator_main.cpp (SDL event pump).

#include <HalGPIO.h>

#include <SDL.h>

#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {
constexpr size_t kButtonCount = 7;

struct ButtonStates {
  bool pressed = false;
  bool wasPressed = false;
  bool wasReleased = false;
  unsigned long pressedAtMs = 0;
};

std::mutex& state_mutex() {
  static std::mutex m;
  return m;
}

ButtonStates g_buttons[kButtonCount];
unsigned long g_lastUpdateMs = 0;
}  // namespace

namespace simulator {
// Called from simulator_main.cpp on each SDL key event.
void injectButton(uint8_t buttonIndex, bool down) {
  if (buttonIndex >= kButtonCount) return;
  std::lock_guard<std::mutex> lock(state_mutex());
  ButtonStates& b = g_buttons[buttonIndex];
  if (down && !b.pressed) {
    b.pressed = true;
    b.wasPressed = true;
    b.pressedAtMs = static_cast<unsigned long>(SDL_GetTicks());
  } else if (!down && b.pressed) {
    b.pressed = false;
    b.wasReleased = true;
  }
}
}  // namespace simulator

void HalGPIO::begin() {}

void HalGPIO::update() {
  // wasPressed/wasReleased are edge-triggered and consumed by the next isPressed/wasPressed read cycle.
  // CrossPoint's contract is: after update(), wasPressed/wasReleased reflect events since the last update().
  // We swap-clear here.
  std::lock_guard<std::mutex> lock(state_mutex());
  for (auto& b : g_buttons) {
    // Keep "wasPressed"/"wasReleased" sticky until next update().
    // simulator::injectButton already set them; we just bump the timestamp.
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
  bool v = g_buttons[buttonIndex].wasPressed;
  g_buttons[buttonIndex].wasPressed = false;
  return v;
}

bool HalGPIO::wasAnyPressed() const {
  std::lock_guard<std::mutex> lock(state_mutex());
  for (auto& b : g_buttons) {
    if (b.wasPressed) return true;
  }
  return false;
}

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  if (buttonIndex >= kButtonCount) return false;
  std::lock_guard<std::mutex> lock(state_mutex());
  bool v = g_buttons[buttonIndex].wasReleased;
  g_buttons[buttonIndex].wasReleased = false;
  return v;
}

bool HalGPIO::wasAnyReleased() const {
  std::lock_guard<std::mutex> lock(state_mutex());
  for (auto& b : g_buttons) {
    if (b.wasReleased) return true;
  }
  return false;
}

unsigned long HalGPIO::getHeldTime() const {
  std::lock_guard<std::mutex> lock(state_mutex());
  unsigned long now = static_cast<unsigned long>(SDL_GetTicks());
  unsigned long maxHeld = 0;
  for (auto& b : g_buttons) {
    if (b.pressed && b.pressedAtMs > 0) {
      unsigned long held = now - b.pressedAtMs;
      if (held > maxHeld) maxHeld = held;
    }
  }
  return maxHeld;
}

void HalGPIO::startDeepSleep() { std::exit(0); }

void HalGPIO::verifyPowerButtonWakeup(uint16_t /*requiredDurationMs*/, bool /*shortPressAllowed*/) {
  // No-op: in the simulator we always proceed past the boot wakeup gate.
}

bool HalGPIO::isUsbConnected() const { return true; }
bool HalGPIO::wasUsbStateChanged() const { return false; }

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const { return WakeupReason::Other; }

HalGPIO gpio;
