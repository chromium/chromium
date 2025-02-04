#ifndef icu4x_LocaleParseError_HPP
#define icu4x_LocaleParseError_HPP

#include "LocaleParseError.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
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
      abort();
  }
}
#endif // icu4x_LocaleParseError_HPP
