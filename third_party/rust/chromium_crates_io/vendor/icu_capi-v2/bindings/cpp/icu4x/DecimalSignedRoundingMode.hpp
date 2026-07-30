#ifndef ICU4X_DecimalSignedRoundingMode_HPP
#define ICU4X_DecimalSignedRoundingMode_HPP

#include "DecimalSignedRoundingMode.d.hpp"

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

} // namespace capi
} // namespace

inline icu4x::capi::DecimalSignedRoundingMode icu4x::DecimalSignedRoundingMode::AsFFI() const {
    return static_cast<icu4x::capi::DecimalSignedRoundingMode>(value);
}

inline icu4x::DecimalSignedRoundingMode icu4x::DecimalSignedRoundingMode::FromFFI(icu4x::capi::DecimalSignedRoundingMode c_enum) {
    switch (c_enum) {
        case icu4x::capi::DecimalSignedRoundingMode_Expand:
        case icu4x::capi::DecimalSignedRoundingMode_Trunc:
        case icu4x::capi::DecimalSignedRoundingMode_HalfExpand:
        case icu4x::capi::DecimalSignedRoundingMode_HalfTrunc:
        case icu4x::capi::DecimalSignedRoundingMode_HalfEven:
        case icu4x::capi::DecimalSignedRoundingMode_Ceil:
        case icu4x::capi::DecimalSignedRoundingMode_Floor:
        case icu4x::capi::DecimalSignedRoundingMode_HalfCeil:
        case icu4x::capi::DecimalSignedRoundingMode_HalfFloor:
            return static_cast<icu4x::DecimalSignedRoundingMode::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DecimalSignedRoundingMode_HPP
