#ifndef icu4x_CollatorNumericOrdering_HPP
#define icu4x_CollatorNumericOrdering_HPP

#include "CollatorNumericOrdering.d.hpp"

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

inline icu4x::capi::CollatorNumericOrdering icu4x::CollatorNumericOrdering::AsFFI() const {
  return static_cast<icu4x::capi::CollatorNumericOrdering>(value);
}

inline icu4x::CollatorNumericOrdering icu4x::CollatorNumericOrdering::FromFFI(icu4x::capi::CollatorNumericOrdering c_enum) {
  switch (c_enum) {
    case icu4x::capi::CollatorNumericOrdering_Off:
    case icu4x::capi::CollatorNumericOrdering_On:
      return static_cast<icu4x::CollatorNumericOrdering::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_CollatorNumericOrdering_HPP
