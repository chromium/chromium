#ifndef icu4x_CollatorNumeric_HPP
#define icu4x_CollatorNumeric_HPP

#include "CollatorNumeric.d.hpp"

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

inline icu4x::capi::CollatorNumeric icu4x::CollatorNumeric::AsFFI() const {
  return static_cast<icu4x::capi::CollatorNumeric>(value);
}

inline icu4x::CollatorNumeric icu4x::CollatorNumeric::FromFFI(icu4x::capi::CollatorNumeric c_enum) {
  switch (c_enum) {
    case icu4x::capi::CollatorNumeric_Off:
    case icu4x::capi::CollatorNumeric_On:
      return static_cast<icu4x::CollatorNumeric::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_CollatorNumeric_HPP
