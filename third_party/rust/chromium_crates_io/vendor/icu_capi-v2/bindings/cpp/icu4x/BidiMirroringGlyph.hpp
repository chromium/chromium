#ifndef ICU4X_BidiMirroringGlyph_HPP
#define ICU4X_BidiMirroringGlyph_HPP

#include "BidiMirroringGlyph.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "BidiPairedBracketType.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::BidiMirroringGlyph icu4x_BidiMirroringGlyph_for_char_mv1(char32_t ch);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::BidiMirroringGlyph icu4x::BidiMirroringGlyph::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_BidiMirroringGlyph_for_char_mv1(ch);
    return icu4x::BidiMirroringGlyph::FromFFI(result);
}


inline icu4x::capi::BidiMirroringGlyph icu4x::BidiMirroringGlyph::AsFFI() const {
    return icu4x::capi::BidiMirroringGlyph {
        /* .mirroring_glyph = */ mirroring_glyph.has_value() ? (icu4x::diplomat::capi::OptionChar{ { mirroring_glyph.value() }, true }) : (icu4x::diplomat::capi::OptionChar{ {}, false }),
        /* .mirrored = */ mirrored,
        /* .paired_bracket_type = */ paired_bracket_type.AsFFI(),
    };
}

inline icu4x::BidiMirroringGlyph icu4x::BidiMirroringGlyph::FromFFI(icu4x::capi::BidiMirroringGlyph c_struct) {
    return icu4x::BidiMirroringGlyph {
        /* .mirroring_glyph = */ c_struct.mirroring_glyph.is_ok ? std::optional(c_struct.mirroring_glyph.ok) : std::nullopt,
        /* .mirrored = */ c_struct.mirrored,
        /* .paired_bracket_type = */ icu4x::BidiPairedBracketType::FromFFI(c_struct.paired_bracket_type),
    };
}


#endif // ICU4X_BidiMirroringGlyph_HPP
