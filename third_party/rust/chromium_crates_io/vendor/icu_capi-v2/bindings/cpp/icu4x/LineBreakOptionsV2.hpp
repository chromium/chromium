#ifndef ICU4X_LineBreakOptionsV2_HPP
#define ICU4X_LineBreakOptionsV2_HPP

#include "LineBreakOptionsV2.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "LineBreakStrictness.hpp"
#include "LineBreakWordOption.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::LineBreakOptionsV2 icu4x::LineBreakOptionsV2::AsFFI() const {
    return icu4x::capi::LineBreakOptionsV2 {
        /* .strictness = */ strictness.has_value() ? (icu4x::capi::LineBreakStrictness_option{ { strictness.value().AsFFI() }, true }) : (icu4x::capi::LineBreakStrictness_option{ {}, false }),
        /* .word_option = */ word_option.has_value() ? (icu4x::capi::LineBreakWordOption_option{ { word_option.value().AsFFI() }, true }) : (icu4x::capi::LineBreakWordOption_option{ {}, false }),
    };
}

inline icu4x::LineBreakOptionsV2 icu4x::LineBreakOptionsV2::FromFFI(icu4x::capi::LineBreakOptionsV2 c_struct) {
    return icu4x::LineBreakOptionsV2 {
        /* .strictness = */ c_struct.strictness.is_ok ? std::optional(icu4x::LineBreakStrictness::FromFFI(c_struct.strictness.ok)) : std::nullopt,
        /* .word_option = */ c_struct.word_option.is_ok ? std::optional(icu4x::LineBreakWordOption::FromFFI(c_struct.word_option.ok)) : std::nullopt,
    };
}


#endif // ICU4X_LineBreakOptionsV2_HPP
