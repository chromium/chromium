#ifndef ICU4X_DecimalParseError_HPP
#define ICU4X_DecimalParseError_HPP

#include "DecimalParseError.d.hpp"

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

inline icu4x::capi::DecimalParseError icu4x::DecimalParseError::AsFFI() const {
    return static_cast<icu4x::capi::DecimalParseError>(value);
}

inline icu4x::DecimalParseError icu4x::DecimalParseError::FromFFI(icu4x::capi::DecimalParseError c_enum) {
    switch (c_enum) {
        case icu4x::capi::DecimalParseError_Unknown:
        case icu4x::capi::DecimalParseError_Limit:
        case icu4x::capi::DecimalParseError_Syntax:
            return static_cast<icu4x::DecimalParseError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DecimalParseError_HPP
