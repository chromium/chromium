#ifndef icu4x_YearStyle_HPP
#define icu4x_YearStyle_HPP

#include "YearStyle.d.hpp"

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

inline icu4x::capi::YearStyle icu4x::YearStyle::AsFFI() const {
  return static_cast<icu4x::capi::YearStyle>(value);
}

inline icu4x::YearStyle icu4x::YearStyle::FromFFI(icu4x::capi::YearStyle c_enum) {
  switch (c_enum) {
    case icu4x::capi::YearStyle_Auto:
    case icu4x::capi::YearStyle_Full:
    case icu4x::capi::YearStyle_WithEra:
      return static_cast<icu4x::YearStyle::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_YearStyle_HPP
