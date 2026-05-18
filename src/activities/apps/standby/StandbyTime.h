#pragma once

#include <cstdint>

// Shared, session-scoped time helpers used by Standby and its faces. The NTP
// "is synced" flag lives in this translation unit so that faces can read it
// without depending on StandbyActivity internals. State resets to "not synced"
// on every cold boot (the device has no battery-backed RTC).
namespace standby_time {

// True if NTP successfully synced during this boot session.
bool isSynced();

// Setter called by StandbyActivity::finishTimeSync() when NTP completes.
void setSynced(bool v);

// Return the current wall-clock HH and MM. When isSynced() is false the result
// is a fallback computed by ticking forward from a plausible start time using
// the caller-supplied millis() anchor (`fallbackStartMs`), so the display looks
// alive instead of stuck at 00:00.
void getNowHHMM(uint32_t fallbackStartMs, unsigned& hh, unsigned& mm);

// Minute-boundary tick used by faces to decide "did the minute change since
// last render?". Same fallback semantics as getNowHHMM().
uint32_t getMinuteTick(uint32_t fallbackStartMs);

}  // namespace standby_time
