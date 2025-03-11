#ifndef icu4x_DateTimeAlignment_HPP
#define icu4x_DateTimeAlignment_HPP

#include "DateTimeAlignment.d.hpp"

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

inline icu4x::capi::DateTimeAlignment icu4x::DateTimeAlignment::AsFFI() const {
  return static_cast<icu4x::capi::DateTimeAlignment>(value);
}

inline icu4x::DateTimeAlignment icu4x::DateTimeAlignment::FromFFI(icu4x::capi::DateTimeAlignment c_enum) {
  switch (c_enum) {
    case icu4x::capi::DateTimeAlignment_Auto:
    case icu4x::capi::DateTimeAlignment_Column:
      return static_cast<icu4x::DateTimeAlignment::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_DateTimeAlignment_HPP
