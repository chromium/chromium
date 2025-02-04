#ifndef icu4x_TrailingCase_HPP
#define icu4x_TrailingCase_HPP

#include "TrailingCase.d.hpp"

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

inline icu4x::capi::TrailingCase icu4x::TrailingCase::AsFFI() const {
  return static_cast<icu4x::capi::TrailingCase>(value);
}

inline icu4x::TrailingCase icu4x::TrailingCase::FromFFI(icu4x::capi::TrailingCase c_enum) {
  switch (c_enum) {
    case icu4x::capi::TrailingCase_Lower:
    case icu4x::capi::TrailingCase_Unchanged:
      return static_cast<icu4x::TrailingCase::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_TrailingCase_HPP
