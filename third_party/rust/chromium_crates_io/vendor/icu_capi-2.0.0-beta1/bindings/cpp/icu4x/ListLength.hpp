#ifndef icu4x_ListLength_HPP
#define icu4x_ListLength_HPP

#include "ListLength.d.hpp"

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

inline icu4x::capi::ListLength icu4x::ListLength::AsFFI() const {
  return static_cast<icu4x::capi::ListLength>(value);
}

inline icu4x::ListLength icu4x::ListLength::FromFFI(icu4x::capi::ListLength c_enum) {
  switch (c_enum) {
    case icu4x::capi::ListLength_Wide:
    case icu4x::capi::ListLength_Short:
    case icu4x::capi::ListLength_Narrow:
      return static_cast<icu4x::ListLength::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_ListLength_HPP
