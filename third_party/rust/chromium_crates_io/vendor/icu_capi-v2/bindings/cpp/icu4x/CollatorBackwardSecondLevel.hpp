#ifndef icu4x_CollatorBackwardSecondLevel_HPP
#define icu4x_CollatorBackwardSecondLevel_HPP

#include "CollatorBackwardSecondLevel.d.hpp"

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

inline icu4x::capi::CollatorBackwardSecondLevel icu4x::CollatorBackwardSecondLevel::AsFFI() const {
  return static_cast<icu4x::capi::CollatorBackwardSecondLevel>(value);
}

inline icu4x::CollatorBackwardSecondLevel icu4x::CollatorBackwardSecondLevel::FromFFI(icu4x::capi::CollatorBackwardSecondLevel c_enum) {
  switch (c_enum) {
    case icu4x::capi::CollatorBackwardSecondLevel_Off:
    case icu4x::capi::CollatorBackwardSecondLevel_On:
      return static_cast<icu4x::CollatorBackwardSecondLevel::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_CollatorBackwardSecondLevel_HPP
