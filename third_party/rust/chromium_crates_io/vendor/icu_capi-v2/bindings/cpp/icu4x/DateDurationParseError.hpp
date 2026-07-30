#ifndef ICU4X_DateDurationParseError_HPP
#define ICU4X_DateDurationParseError_HPP

#include "DateDurationParseError.d.hpp"

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

inline icu4x::capi::DateDurationParseError icu4x::DateDurationParseError::AsFFI() const {
    return static_cast<icu4x::capi::DateDurationParseError>(value);
}

inline icu4x::DateDurationParseError icu4x::DateDurationParseError::FromFFI(icu4x::capi::DateDurationParseError c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateDurationParseError_InvalidStructure:
        case icu4x::capi::DateDurationParseError_TimeNotSupported:
        case icu4x::capi::DateDurationParseError_MissingValue:
        case icu4x::capi::DateDurationParseError_DuplicateUnit:
        case icu4x::capi::DateDurationParseError_NumberOverflow:
        case icu4x::capi::DateDurationParseError_PlusNotAllowed:
            return static_cast<icu4x::DateDurationParseError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateDurationParseError_HPP
