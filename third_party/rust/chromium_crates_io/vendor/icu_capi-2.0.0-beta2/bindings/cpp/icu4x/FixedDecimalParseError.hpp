#ifndef icu4x_FixedDecimalParseError_HPP
#define icu4x_FixedDecimalParseError_HPP

#include "FixedDecimalParseError.d.hpp"

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

inline icu4x::capi::FixedDecimalParseError icu4x::FixedDecimalParseError::AsFFI() const {
  return static_cast<icu4x::capi::FixedDecimalParseError>(value);
}

inline icu4x::FixedDecimalParseError icu4x::FixedDecimalParseError::FromFFI(icu4x::capi::FixedDecimalParseError c_enum) {
  switch (c_enum) {
    case icu4x::capi::FixedDecimalParseError_Unknown:
    case icu4x::capi::FixedDecimalParseError_Limit:
    case icu4x::capi::FixedDecimalParseError_Syntax:
      return static_cast<icu4x::FixedDecimalParseError::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_FixedDecimalParseError_HPP
