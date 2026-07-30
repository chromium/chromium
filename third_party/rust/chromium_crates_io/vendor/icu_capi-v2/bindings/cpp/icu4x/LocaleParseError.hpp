#ifndef ICU4X_LocaleParseError_HPP
#define ICU4X_LocaleParseError_HPP

#include "LocaleParseError.d.hpp"

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

inline icu4x::capi::LocaleParseError icu4x::LocaleParseError::AsFFI() const {
    return static_cast<icu4x::capi::LocaleParseError>(value);
}

inline icu4x::LocaleParseError icu4x::LocaleParseError::FromFFI(icu4x::capi::LocaleParseError c_enum) {
    switch (c_enum) {
        case icu4x::capi::LocaleParseError_Unknown:
        case icu4x::capi::LocaleParseError_Language:
        case icu4x::capi::LocaleParseError_Subtag:
        case icu4x::capi::LocaleParseError_Extension:
            return static_cast<icu4x::LocaleParseError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_LocaleParseError_HPP
