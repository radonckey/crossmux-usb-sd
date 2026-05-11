#pragma once

#include <SDL.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace simulator {

// Singleton owning the SDL window + texture used by HalDisplay_native to present frames.
// Lives in the main thread; HalDisplay calls pushFramebuffer() from whichever thread
// renders, which atomically swaps a back-buffer that the main loop picks up.
class SimulatorWindow {
 public:
  static SimulatorWindow& instance();

  // Open the SDL window. Must be called from the main thread before any pushFramebuffer().
  bool open(const char* title, int scale);

  // Forward a 1-bpp framebuffer (48000 bytes, 800×480, MSB-first). Thread-safe.
  void pushFramebuffer(const uint8_t* bw1bpp);

  // Pump pending frame to the screen if a new one is available. Called from main loop.
  void presentIfDirty();

  void close();

  SDL_Window* sdlWindow() const { return window_; }

 private:
  SimulatorWindow() = default;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  int scale_ = 1;

  std::mutex pendingMutex_;
  std::vector<uint8_t> pending_;
  bool pendingDirty_ = false;
};

// Forwarded by simulator_main.cpp on each SDL key event.
// buttonIndex matches HalGPIO::BTN_* constants.
void injectButton(uint8_t buttonIndex, bool down);

}  // namespace simulator
