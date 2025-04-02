#ifndef icu4x_DecimalGroupingStrategy_HPP
#define icu4x_DecimalGroupingStrategy_HPP

#include "DecimalGroupingStrategy.d.hpp"

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

inline icu4x::capi::DecimalGroupingStrategy icu4x::DecimalGroupingStrategy::AsFFI() const {
  return static_cast<icu4x::capi::DecimalGroupingStrategy>(value);
}

inline icu4x::DecimalGroupingStrategy icu4x::DecimalGroupingStrategy::FromFFI(icu4x::capi::DecimalGroupingStrategy c_enum) {
  switch (c_enum) {
    case icu4x::capi::DecimalGroupingStrategy_Auto:
    case icu4x::capi::DecimalGroupingStrategy_Never:
    case icu4x::capi::DecimalGroupingStrategy_Always:
    case icu4x::capi::DecimalGroupingStrategy_Min2:
      return static_cast<icu4x::DecimalGroupingStrategy::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_DecimalGroupingStrategy_HPP
