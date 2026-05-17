#pragma once

#include <cstdint>
#include <memory>

#include "../../Activity.h"
#include "StandbyFace.h"

class StandbyActivity final : public Activity {
 public:
  explicit StandbyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Standby", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  void onExit() override;

  // Tight loop and prevent auto-sleep while WiFi/NTP sync is in progress.
  bool preventAutoSleep() override { return syncState_ != SyncState::Idle; }
  bool skipLoopDelay() override { return syncState_ != SyncState::Idle; }

 private:
  enum class SyncState : uint8_t {
    Idle,            // No sync running (either succeeded or skipped/failed)
    WifiConnecting,  // WiFi.begin() called, polling WiFi.status()
    NtpSyncing,      // SNTP started, polling sntp_get_sync_status()
  };

  enum class DisplayMode : uint8_t {
    Normal,     // Header + button hints + face content
    Immersive,  // Face content only (after 5s idle)
    Sleep,      // Light-sleep loop, only power button wakes (after 35s idle)
  };

  std::unique_ptr<StandbyFace> currentFace_;
  uint8_t faceIndex_ = 0;
  SyncState syncState_ = SyncState::Idle;
  uint32_t syncStartMs_ = 0;
  DisplayMode mode_ = DisplayMode::Normal;
  uint32_t lastInputMs_ = 0;

  void switchFace(int8_t delta);
  void startTimeSync();
  void pumpTimeSync();
  void finishTimeSync();
  void enterSleep();
};
