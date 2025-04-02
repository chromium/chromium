#ifndef icu4x_CollatorCaseFirst_HPP
#define icu4x_CollatorCaseFirst_HPP

#include "CollatorCaseFirst.d.hpp"

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

inline icu4x::capi::CollatorCaseFirst icu4x::CollatorCaseFirst::AsFFI() const {
  return static_cast<icu4x::capi::CollatorCaseFirst>(value);
}

inline icu4x::CollatorCaseFirst icu4x::CollatorCaseFirst::FromFFI(icu4x::capi::CollatorCaseFirst c_enum) {
  switch (c_enum) {
    case icu4x::capi::CollatorCaseFirst_Off:
    case icu4x::capi::CollatorCaseFirst_Lower:
    case icu4x::capi::CollatorCaseFirst_Upper:
      return static_cast<icu4x::CollatorCaseFirst::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_CollatorCaseFirst_HPP
