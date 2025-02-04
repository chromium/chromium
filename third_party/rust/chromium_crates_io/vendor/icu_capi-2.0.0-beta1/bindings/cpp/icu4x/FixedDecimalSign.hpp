#ifndef icu4x_FixedDecimalSign_HPP
#define icu4x_FixedDecimalSign_HPP

#include "FixedDecimalSign.d.hpp"

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

inline icu4x::capi::FixedDecimalSign icu4x::FixedDecimalSign::AsFFI() const {
  return static_cast<icu4x::capi::FixedDecimalSign>(value);
}

inline icu4x::FixedDecimalSign icu4x::FixedDecimalSign::FromFFI(icu4x::capi::FixedDecimalSign c_enum) {
  switch (c_enum) {
    case icu4x::capi::FixedDecimalSign_None:
    case icu4x::capi::FixedDecimalSign_Negative:
    case icu4x::capi::FixedDecimalSign_Positive:
      return static_cast<icu4x::FixedDecimalSign::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_FixedDecimalSign_HPP
