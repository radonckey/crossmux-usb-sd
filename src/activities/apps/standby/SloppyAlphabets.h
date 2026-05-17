#pragma once

#include <cstdint>

namespace sloppy {

constexpr int16_t DIGIT_W = 100;
constexpr int16_t DIGIT_H = 160;

enum class CmdKind : uint8_t {
  Move = 0,
  Cubic = 1,
};

struct Cmd {
  CmdKind kind;
  int16_t x0, y0;
  int16_t x1, y1;
  int16_t x2, y2;
};

struct Glyph {
  const Cmd* cmds;
  uint8_t cmdCount;
};

struct Alphabet {
  const Glyph* glyphs;  // 10 entries, indexed by digit 0..9
  Glyph one_plain;      // Ornament-free variant of '1' — straight stem only.
  const char* name;
};

enum class AlphabetId : uint8_t {
  Geometric = 0,
  Script,
  Marker,
  Count,
};

const Alphabet& getAlphabet(AlphabetId id);

// Upper bound on (M points + 3 * C points) summed across one glyph's commands.
// Across geometric/script/marker the max is digit '9' at 17 seeds. 20 leaves
// a small headroom while keeping Seeds.glyphSeeds at 20*10*2 = 400 bytes.
constexpr uint8_t MAX_GLYPH_CONTROL_POINTS = 20;

}  // namespace sloppy
