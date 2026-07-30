#ifndef ICU4X_DateTimeWriteError_HPP
#define ICU4X_DateTimeWriteError_HPP

#include "DateTimeWriteError.d.hpp"

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

inline icu4x::capi::DateTimeWriteError icu4x::DateTimeWriteError::AsFFI() const {
    return static_cast<icu4x::capi::DateTimeWriteError>(value);
}

inline icu4x::DateTimeWriteError icu4x::DateTimeWriteError::FromFFI(icu4x::capi::DateTimeWriteError c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateTimeWriteError_Unknown:
        case icu4x::capi::DateTimeWriteError_MissingTimeZoneVariant:
            return static_cast<icu4x::DateTimeWriteError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateTimeWriteError_HPP
