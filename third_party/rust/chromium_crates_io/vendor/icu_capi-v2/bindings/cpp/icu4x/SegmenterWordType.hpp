#ifndef ICU4X_SegmenterWordType_HPP
#define ICU4X_SegmenterWordType_HPP

#include "SegmenterWordType.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    bool icu4x_SegmenterWordType_is_word_like_mv1(icu4x::capi::SegmenterWordType self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::SegmenterWordType icu4x::SegmenterWordType::AsFFI() const {
    return static_cast<icu4x::capi::SegmenterWordType>(value);
}

inline icu4x::SegmenterWordType icu4x::SegmenterWordType::FromFFI(icu4x::capi::SegmenterWordType c_enum) {
    switch (c_enum) {
        case icu4x::capi::SegmenterWordType_None:
        case icu4x::capi::SegmenterWordType_Number:
        case icu4x::capi::SegmenterWordType_Letter:
            return static_cast<icu4x::SegmenterWordType::Value>(c_enum);
        default:
            std::abort();
    }
}

inline bool icu4x::SegmenterWordType::is_word_like() const {
    auto result = icu4x::capi::icu4x_SegmenterWordType_is_word_like_mv1(this->AsFFI());
    return result;
}
#endif // ICU4X_SegmenterWordType_HPP
