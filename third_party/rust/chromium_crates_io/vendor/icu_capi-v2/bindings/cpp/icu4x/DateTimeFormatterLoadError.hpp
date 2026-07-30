#ifndef ICU4X_DateTimeFormatterLoadError_HPP
#define ICU4X_DateTimeFormatterLoadError_HPP

#include "DateTimeFormatterLoadError.d.hpp"

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

inline icu4x::capi::DateTimeFormatterLoadError icu4x::DateTimeFormatterLoadError::AsFFI() const {
    return static_cast<icu4x::capi::DateTimeFormatterLoadError>(value);
}

inline icu4x::DateTimeFormatterLoadError icu4x::DateTimeFormatterLoadError::FromFFI(icu4x::capi::DateTimeFormatterLoadError c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateTimeFormatterLoadError_Unknown:
        case icu4x::capi::DateTimeFormatterLoadError_InvalidDateFields:
        case icu4x::capi::DateTimeFormatterLoadError_UnsupportedLength:
        case icu4x::capi::DateTimeFormatterLoadError_ConflictingField:
        case icu4x::capi::DateTimeFormatterLoadError_FormatterTooSpecific:
        case icu4x::capi::DateTimeFormatterLoadError_DataMarkerNotFound:
        case icu4x::capi::DateTimeFormatterLoadError_DataIdentifierNotFound:
        case icu4x::capi::DateTimeFormatterLoadError_DataInvalidRequest:
        case icu4x::capi::DateTimeFormatterLoadError_DataInconsistentData:
        case icu4x::capi::DateTimeFormatterLoadError_DataDowncast:
        case icu4x::capi::DateTimeFormatterLoadError_DataDeserialize:
        case icu4x::capi::DateTimeFormatterLoadError_DataCustom:
        case icu4x::capi::DateTimeFormatterLoadError_DataIo:
            return static_cast<icu4x::DateTimeFormatterLoadError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateTimeFormatterLoadError_HPP
