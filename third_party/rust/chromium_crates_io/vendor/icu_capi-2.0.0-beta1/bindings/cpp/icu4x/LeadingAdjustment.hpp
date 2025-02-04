#ifndef icu4x_LeadingAdjustment_HPP
#define icu4x_LeadingAdjustment_HPP

#include "LeadingAdjustment.d.hpp"

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

inline icu4x::capi::LeadingAdjustment icu4x::LeadingAdjustment::AsFFI() const {
  return static_cast<icu4x::capi::LeadingAdjustment>(value);
}

inline icu4x::LeadingAdjustment icu4x::LeadingAdjustment::FromFFI(icu4x::capi::LeadingAdjustment c_enum) {
  switch (c_enum) {
    case icu4x::capi::LeadingAdjustment_Auto:
    case icu4x::capi::LeadingAdjustment_None:
    case icu4x::capi::LeadingAdjustment_ToCased:
      return static_cast<icu4x::LeadingAdjustment::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_LeadingAdjustment_HPP
