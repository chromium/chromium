#ifndef ICU4X_Rfc9557ParseError_HPP
#define ICU4X_Rfc9557ParseError_HPP

#include "Rfc9557ParseError.d.hpp"

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

inline icu4x::capi::Rfc9557ParseError icu4x::Rfc9557ParseError::AsFFI() const {
    return static_cast<icu4x::capi::Rfc9557ParseError>(value);
}

inline icu4x::Rfc9557ParseError icu4x::Rfc9557ParseError::FromFFI(icu4x::capi::Rfc9557ParseError c_enum) {
    switch (c_enum) {
        case icu4x::capi::Rfc9557ParseError_Unknown:
        case icu4x::capi::Rfc9557ParseError_InvalidSyntax:
        case icu4x::capi::Rfc9557ParseError_OutOfRange:
        case icu4x::capi::Rfc9557ParseError_MissingFields:
        case icu4x::capi::Rfc9557ParseError_UnknownCalendar:
            return static_cast<icu4x::Rfc9557ParseError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_Rfc9557ParseError_HPP
