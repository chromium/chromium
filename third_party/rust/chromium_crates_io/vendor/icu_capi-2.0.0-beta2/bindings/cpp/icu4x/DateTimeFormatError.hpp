#ifndef icu4x_DateTimeFormatError_HPP
#define icu4x_DateTimeFormatError_HPP

#include "DateTimeFormatError.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::DateTimeFormatError icu4x::DateTimeFormatError::AsFFI() const {
  return static_cast<icu4x::capi::DateTimeFormatError>(value);
}

inline icu4x::DateTimeFormatError icu4x::DateTimeFormatError::FromFFI(icu4x::capi::DateTimeFormatError c_enum) {
  switch (c_enum) {
    case icu4x::capi::DateTimeFormatError_Unknown:
    case icu4x::capi::DateTimeFormatError_MissingInputField:
    case icu4x::capi::DateTimeFormatError_ZoneInfoMissingFields:
    case icu4x::capi::DateTimeFormatError_InvalidEra:
    case icu4x::capi::DateTimeFormatError_InvalidMonthCode:
    case icu4x::capi::DateTimeFormatError_InvalidCyclicYear:
    case icu4x::capi::DateTimeFormatError_NamesNotLoaded:
    case icu4x::capi::DateTimeFormatError_DecimalFormatterNotLoaded:
    case icu4x::capi::DateTimeFormatError_UnsupportedField:
      return static_cast<icu4x::DateTimeFormatError::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_DateTimeFormatError_HPP
