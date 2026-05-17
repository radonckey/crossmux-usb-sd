#include "StandbyActivity.h"

#include <Arduino.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <InputManager.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <esp_system.h>

#include <string>

#include "SloppyClockFace.h"
#include "StandbyTime.h"
#include "WifiCredentialStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr long kTzOffsetSec = 8 * 3600;      // UTC+8 (Beijing)
constexpr uint32_t kWifiTimeoutMs = 15000u;  // Same as WifiSelectionActivity
constexpr uint32_t kNtpTimeoutMs = 12000u;   // SNTP poll budget (multi-server DNS + handshake)

// Face factory table. Add new faces by appending a row here and including the
// corresponding header above — StandbyActivity needs no other change.
struct FaceEntry {
  std::unique_ptr<StandbyFace> (*create)();
};
constexpr FaceEntry kFaces[] = {
    {[]() -> std::unique_ptr<StandbyFace> { return makeUniqueNoThrow<SloppyClockFace>(); }},
};
constexpr uint8_t kFaceCount = static_cast<uint8_t>(sizeof(kFaces) / sizeof(kFaces[0]));

// Bottom-center page indicator dots. One dot per face, filled for the current
// face. Drawn only in Normal mode — hidden once we go Immersive / Sleep.
constexpr int kDotDiameter = 6;
constexpr int kDotSpacing = 14;     // gap between dot centers
constexpr int kDotBottomInset = 24; // distance from bottom edge to top of dots

void drawFaceDots(const GfxRenderer& renderer, int sw, int sh, uint8_t total, uint8_t current) {
  if (total == 0) return;
  const int totalWidth = total * kDotDiameter + (total - 1) * kDotSpacing;
  const int startX = (sw - totalWidth) / 2;
  const int y = sh - kDotBottomInset;
  for (uint8_t i = 0; i < total; ++i) {
    const int x = startX + i * (kDotDiameter + kDotSpacing);
    if (i == current) {
      renderer.fillRoundedRect(x, y, kDotDiameter, kDotDiameter, kDotDiameter / 2, Color::Black);
    } else {
      // 1 px outlined dot for inactive faces.
      renderer.drawRoundedRect(x, y, kDotDiameter, kDotDiameter, /*lineWidth=*/1, kDotDiameter / 2,
                               /*state=*/true);
    }
  }
}

}  // namespace

void StandbyActivity::onEnter() {
  Activity::onEnter();
  LOG_DBG("STANDBY", "onEnter free heap=%u", static_cast<unsigned>(ESP.getFreeHeap()));
  faceIndex_ = 0;
  currentFace_ = kFaces[faceIndex_].create();
  if (!currentFace_) {
    LOG_ERR("STANDBY", "OOM allocating face");
    activityManager.goToApps();
    return;
  }
  currentFace_->onEnter();
  mode_ = DisplayMode::Normal;
  lastInputMs_ = millis();
  startTimeSync();
  requestUpdate();
}

void StandbyActivity::onExit() {
  if (syncState_ != SyncState::Idle) {
    // User backed out while sync was running — tear down cleanly.
    if (esp_sntp_enabled()) esp_sntp_stop();
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_OFF);
    syncState_ = SyncState::Idle;
  }
  // Defensive: if the activity is destroyed mid-Sleep (rare — enterSleep
  // normally blocks until the power button), clean up sleep wake sources so
  // they don't leak into the next activity's sleep path.
  if (mode_ == DisplayMode::Sleep) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  }
  if (currentFace_) {
    currentFace_->onExit();
    currentFace_.reset();
  }
  LOG_DBG("STANDBY", "onExit free heap=%u", static_cast<unsigned>(ESP.getFreeHeap()));
  Activity::onExit();
}

void StandbyActivity::switchFace(int8_t delta) {
  if (kFaceCount <= 1) return;
  if (currentFace_) currentFace_->onExit();
  currentFace_.reset();
  faceIndex_ = static_cast<uint8_t>((faceIndex_ + kFaceCount + delta) % kFaceCount);
  currentFace_ = kFaces[faceIndex_].create();
  if (!currentFace_) {
    LOG_ERR("STANDBY", "OOM switching face");
    activityManager.goToApps();
    return;
  }
  currentFace_->onEnter();
  requestUpdate();
}

void StandbyActivity::startTimeSync() {
  if (standby_time::isSynced()) return;

  // Load and look up saved credentials. SD I/O shares SPI with the e-ink panel,
  // so we hold a RenderLock for the duration of the file access.
  std::string ssid;
  std::string pass;
  {
    RenderLock lock(*this);
    if (WIFI_STORE.getCredentials().empty()) WIFI_STORE.loadFromFile();
    const std::string& last = WIFI_STORE.getLastConnectedSsid();
    if (last.empty()) {
      LOG_DBG("STANDBY", "No lastConnectedSsid, skipping NTP sync");
      return;
    }
    const WifiCredential* cred = WIFI_STORE.findCredential(last);
    if (!cred) {
      LOG_DBG("STANDBY", "lastConnectedSsid '%s' has no saved credential", last.c_str());
      return;
    }
    ssid = cred->ssid;
    pass = cred->password;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  if (pass.empty()) {
    WiFi.begin(ssid.c_str());
  } else {
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
  syncState_ = SyncState::WifiConnecting;
  syncStartMs_ = millis();
  LOG_DBG("STANDBY", "WiFi connect start: %s", ssid.c_str());
}

void StandbyActivity::pumpTimeSync() {
  if (syncState_ == SyncState::Idle) return;
  const uint32_t elapsed = millis() - syncStartMs_;

  if (syncState_ == SyncState::WifiConnecting) {
    const wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      if (esp_sntp_enabled()) esp_sntp_stop();
#ifdef ENABLE_CHINESE_VERSION
      // China-region servers: Aliyun is the most reliable; Tencent and the NTP
      // Pool CN node act as fallbacks. pool.ntp.org is often blocked or slow
      // inside the mainland.
      configTime(kTzOffsetSec, 0, "ntp.aliyun.com", "ntp.tencent.com", "cn.pool.ntp.org");
#else
      configTime(kTzOffsetSec, 0, "pool.ntp.org");
#endif
      syncState_ = SyncState::NtpSyncing;
      syncStartMs_ = millis();
      LOG_DBG("STANDBY", "WiFi connected, NTP started");
      return;
    }
    if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL || elapsed >= kWifiTimeoutMs) {
      LOG_DBG("STANDBY", "WiFi sync skipped (status=%d, t=%ums)", static_cast<int>(st), static_cast<unsigned>(elapsed));
      finishTimeSync();
    }
    return;
  }

  if (syncState_ == SyncState::NtpSyncing) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      standby_time::setSynced(true);
      LOG_DBG("STANDBY", "NTP synced");
      finishTimeSync();
      return;
    }
    if (elapsed >= kNtpTimeoutMs) {
      LOG_DBG("STANDBY", "NTP timeout");
      finishTimeSync();
    }
  }
}

void StandbyActivity::finishTimeSync() {
  if (esp_sntp_enabled()) esp_sntp_stop();
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  syncState_ = SyncState::Idle;
  requestUpdate();  // Header text and face content may now reflect synced time
}

void StandbyActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.goToApps();
    return;
  }

  // Up/Down: forward "shake" to the current face. For SloppyClock this rerolls
  // the style; future faces may interpret it differently or ignore.
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    lastInputMs_ = millis();
    if (mode_ == DisplayMode::Immersive) {
      mode_ = DisplayMode::Normal;
      requestUpdate();
    } else if (currentFace_) {
      currentFace_->onShake(esp_random() ^ static_cast<uint32_t>(millis()));
      requestUpdate();
    }
    return;
  }

  // Left/Right: cycle through faces. With only one face installed this is a
  // silent no-op (the keystroke is consumed but nothing visible changes).
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
      mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    lastInputMs_ = millis();
    if (mode_ == DisplayMode::Immersive) {
      mode_ = DisplayMode::Normal;
      requestUpdate();
      return;
    }
    const int8_t delta = mappedInput.wasReleased(MappedInputManager::Button::Right) ? +1 : -1;
    switchFace(delta);
    return;
  }

  // Confirm is currently unbound and reserved for future face-specific or
  // chrome-toggle behaviour.

  pumpTimeSync();

  // Mode auto-transitions on idle thresholds:
  //   Normal      → Immersive after 5 s idle
  //   Immersive   → Sleep after 35 s idle, BUT only on battery and only when
  //                 NTP sync is finished. With USB connected we stay in Immersive
  //                 indefinitely (no point sleeping while externally powered).
  const uint32_t idle = millis() - lastInputMs_;
  if (mode_ == DisplayMode::Normal && idle >= 5000u) {
    mode_ = DisplayMode::Immersive;
    requestUpdate();
  } else if (mode_ == DisplayMode::Immersive && idle >= 35000u && syncState_ == SyncState::Idle &&
             !gpio.isUsbConnected()) {
    enterSleep();  // Blocks until power button (or USB plug) wakes us.
    return;
  }

  if (currentFace_ && currentFace_->tick()) {
    requestUpdate();
  }
}

void StandbyActivity::render(RenderLock&&) {
  if (!currentFace_) return;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  renderer.clearScreen();

  // Face renders into the full screen regardless of mode. Chrome (title, battery,
  // dot indicator) is drawn as overlay on top in Normal mode only, so the Face
  // content doesn't re-flow when transitioning to Immersive / Sleep.
  currentFace_->render(renderer, Rect{0, 0, sw, sh});

  if (mode_ != DisplayMode::Normal) {
    renderer.displayBuffer();
    return;
  }

  // Top-center face title (or sync state). Small font, no chrome container,
  // no separator line — Apple Standby-style minimal overlay.
  const char* title = (syncState_ != SyncState::Idle)
                          ? tr(STR_STANDBY_SYNCING)
                          : I18n::getInstance().get(currentFace_->titleId());
  renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding, title, /*black=*/true);

  // Top-right battery icon (no percentage text). Reuses BaseTheme::drawBatteryRight.
  constexpr int kBatW = 16;
  constexpr int kBatH = 12;
  GUI.drawBatteryRight(renderer,
                       Rect{sw - kBatW - metrics.contentSidePadding, metrics.topPadding, kBatW, kBatH},
                       /*showPercentage=*/false);

  drawFaceDots(renderer, sw, sh, kFaceCount, faceIndex_);

  // Standby stays on FAST_REFRESH end-to-end — no full/half waveform flashes.
  // We accept some long-term ghosting in exchange for a calm, non-blinking face.
  renderer.displayBuffer();
}

void StandbyActivity::enterSleep() {
  if (!currentFace_) return;
  mode_ = DisplayMode::Sleep;
  // First Sleep frame uses FAST_REFRESH like everything else in this app.
  requestUpdateAndWait();

  // Real ESP32-C3 light sleep. This is only reached when USB is unplugged
  // (loop() gates Sleep entry on !gpio.isUsbConnected()); on battery the
  // USB Serial JTAG peripheral is quiescent and esp_light_sleep_start() works
  // reliably, yielding ~0.8 mA average power.
  const gpio_num_t powerPin = static_cast<gpio_num_t>(InputManager::POWER_BUTTON_PIN);
  pinMode(powerPin, INPUT_PULLUP);
  gpio_wakeup_enable(powerPin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  LOG_DBG("STANDBY", "sleep start");

  while (true) {
    uint32_t secsToNext = currentFace_->secondsUntilNextWake();
    if (secsToNext == 0) secsToNext = 60;
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(secsToNext) * 1000000ULL);

    LOG_DBG("STANDBY", "light sleep %us", static_cast<unsigned>(secsToNext));
    esp_light_sleep_start();
    const auto cause = esp_sleep_get_wakeup_cause();
    LOG_DBG("STANDBY", "wake cause=%d", static_cast<int>(cause));

    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
      // Power button: drain the press so the main loop doesn't immediately
      // treat it as a still-held button (which could trigger shutdown).
      const uint32_t releaseStart = millis();
      while (digitalRead(powerPin) == LOW && millis() - releaseStart < 2000u) {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
      LOG_DBG("STANDBY", "power button -> exit Sleep");
      break;
    }

    // USB plugged in during Sleep: leave the loop so we revert to Immersive
    // and let the user interact with full-rate rendering on external power.
    if (gpio.isUsbConnected()) {
      LOG_DBG("STANDBY", "USB connected -> exit Sleep");
      break;
    }

    // Timer wake: let the face decide whether content actually changed, then
    // redraw with FAST_REFRESH if needed. We accept some ghosting in exchange
    // for silent, flash-free updates.
    if (currentFace_->tick()) {
      requestUpdateAndWait();
    }
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  gpio_wakeup_disable(powerPin);

  LOG_DBG("STANDBY", "sleep end -> Normal");
  mode_ = DisplayMode::Normal;
  lastInputMs_ = millis();
  requestUpdate();
}
