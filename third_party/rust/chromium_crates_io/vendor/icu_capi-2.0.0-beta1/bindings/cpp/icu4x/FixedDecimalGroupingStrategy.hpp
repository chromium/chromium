#ifndef icu4x_FixedDecimalGroupingStrategy_HPP
#define icu4x_FixedDecimalGroupingStrategy_HPP

#include "FixedDecimalGroupingStrategy.d.hpp"

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

inline icu4x::capi::FixedDecimalGroupingStrategy icu4x::FixedDecimalGroupingStrategy::AsFFI() const {
  return static_cast<icu4x::capi::FixedDecimalGroupingStrategy>(value);
}

inline icu4x::FixedDecimalGroupingStrategy icu4x::FixedDecimalGroupingStrategy::FromFFI(icu4x::capi::FixedDecimalGroupingStrategy c_enum) {
  switch (c_enum) {
    case icu4x::capi::FixedDecimalGroupingStrategy_Auto:
    case icu4x::capi::FixedDecimalGroupingStrategy_Never:
    case icu4x::capi::FixedDecimalGroupingStrategy_Always:
    case icu4x::capi::FixedDecimalGroupingStrategy_Min2:
      return static_cast<icu4x::FixedDecimalGroupingStrategy::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_FixedDecimalGroupingStrategy_HPP
