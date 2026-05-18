#pragma once

#include <cstdint>

#include "SloppyAlphabets.h"
#include "components/themes/BaseTheme.h"  // for ::Rect

class GfxRenderer;

namespace sloppy {

struct Style {
  AlphabetId alphabet;
  float wobble;          // [2..14] template units; Bezier control-point jitter amplitude
  uint8_t strokeWidth;   // [2..7] pixels; passed straight to drawLine, not scaled
  float slantDeg;        // [-18..18]; 0 with 40% probability
  float digitRotateMax;  // [0..12] degrees; per-position rotation jitter cap
  uint8_t digitGap;      // [10..30] template units between adjacent digits in a row
  bool oneIsPlain;       // when true, '1' is drawn as a bare stem (no top hook / base serif)
};

struct PointSeed {
  int8_t dx;  // -127..127  →  dx/127 ∈ [-1, 1]
  int8_t dy;
};

struct PositionSeed {
  int8_t rotJitter;  // -127..127  →  rotation = (j/127) * digitRotateMax degrees
  int8_t sxJitter;   // -127..127  →  sx = 1 + (j/127) * 0.08
  int8_t syJitter;
};

// HH:MM only — 4 digit slots, each gets its own rotation + aspect jitter.
constexpr int kMaxTimeSlots = 4;

struct Seeds {
  // Per-digit jitter seeds, stored flat in command order:
  //   for each Cmd in glyph: Move contributes 1 seed (anchor),
  //                          Cubic contributes 3 seeds (P1, P2, P3).
  PointSeed glyphSeeds[10][MAX_GLYPH_CONTROL_POINTS];
  uint8_t glyphSeedCount[10];
  PositionSeed positions[kMaxTimeSlots];
};

void rollStyle(uint32_t seed, Style& out);
void preRollSeeds(uint32_t seed, const Alphabet& alpha, Seeds& out);

// Renders a multi-line time string within the viewport. Newlines split rows;
// digits ('0'-'9') are rendered, all other characters are skipped. Typical
// input is "HH\nMM". Caller has already clearScreen()-ed.
void draw(GfxRenderer& renderer, const Style& style, const Seeds& seeds, const char* timeStr, Rect viewport);

}  // namespace sloppy
