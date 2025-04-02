#ifndef icu4x_BidiMirroringGlyph_D_HPP
#define icu4x_BidiMirroringGlyph_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "BidiPairedBracketType.d.hpp"

namespace icu4x {
struct BidiMirroringGlyph;
class BidiPairedBracketType;
}


namespace icu4x {
namespace capi {
    struct BidiMirroringGlyph {
      diplomat::capi::OptionChar mirroring_glyph;
      bool mirrored;
      icu4x::capi::BidiPairedBracketType paired_bracket_type;
    };
    
    typedef struct BidiMirroringGlyph_option {union { BidiMirroringGlyph ok; }; bool is_ok; } BidiMirroringGlyph_option;
} // namespace capi
} // namespace


namespace icu4x {
struct BidiMirroringGlyph {
  std::optional<char32_t> mirroring_glyph;
  bool mirrored;
  icu4x::BidiPairedBracketType paired_bracket_type;

  inline static icu4x::BidiMirroringGlyph for_char(char32_t ch);

  inline icu4x::capi::BidiMirroringGlyph AsFFI() const;
  inline static icu4x::BidiMirroringGlyph FromFFI(icu4x::capi::BidiMirroringGlyph c_struct);
};

} // namespace
#endif // icu4x_BidiMirroringGlyph_D_HPP
