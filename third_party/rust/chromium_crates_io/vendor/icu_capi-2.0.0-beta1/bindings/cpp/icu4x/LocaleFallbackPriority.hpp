#ifndef icu4x_LocaleFallbackPriority_HPP
#define icu4x_LocaleFallbackPriority_HPP

#include "LocaleFallbackPriority.d.hpp"

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

inline icu4x::capi::LocaleFallbackPriority icu4x::LocaleFallbackPriority::AsFFI() const {
  return static_cast<icu4x::capi::LocaleFallbackPriority>(value);
}

inline icu4x::LocaleFallbackPriority icu4x::LocaleFallbackPriority::FromFFI(icu4x::capi::LocaleFallbackPriority c_enum) {
  switch (c_enum) {
    case icu4x::capi::LocaleFallbackPriority_Language:
    case icu4x::capi::LocaleFallbackPriority_Region:
      return static_cast<icu4x::LocaleFallbackPriority::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_LocaleFallbackPriority_HPP
