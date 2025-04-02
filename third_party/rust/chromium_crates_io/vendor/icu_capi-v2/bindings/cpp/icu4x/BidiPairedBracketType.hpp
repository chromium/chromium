#ifndef icu4x_BidiPairedBracketType_HPP
#define icu4x_BidiPairedBracketType_HPP

#include "BidiPairedBracketType.d.hpp"

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

inline icu4x::capi::BidiPairedBracketType icu4x::BidiPairedBracketType::AsFFI() const {
  return static_cast<icu4x::capi::BidiPairedBracketType>(value);
}

inline icu4x::BidiPairedBracketType icu4x::BidiPairedBracketType::FromFFI(icu4x::capi::BidiPairedBracketType c_enum) {
  switch (c_enum) {
    case icu4x::capi::BidiPairedBracketType_Open:
    case icu4x::capi::BidiPairedBracketType_Close:
    case icu4x::capi::BidiPairedBracketType_None:
      return static_cast<icu4x::BidiPairedBracketType::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_BidiPairedBracketType_HPP
