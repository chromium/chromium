#ifndef icu4x_FixedDecimalRoundingIncrement_HPP
#define icu4x_FixedDecimalRoundingIncrement_HPP

#include "FixedDecimalRoundingIncrement.d.hpp"

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

inline icu4x::capi::FixedDecimalRoundingIncrement icu4x::FixedDecimalRoundingIncrement::AsFFI() const {
  return static_cast<icu4x::capi::FixedDecimalRoundingIncrement>(value);
}

inline icu4x::FixedDecimalRoundingIncrement icu4x::FixedDecimalRoundingIncrement::FromFFI(icu4x::capi::FixedDecimalRoundingIncrement c_enum) {
  switch (c_enum) {
    case icu4x::capi::FixedDecimalRoundingIncrement_MultiplesOf1:
    case icu4x::capi::FixedDecimalRoundingIncrement_MultiplesOf2:
    case icu4x::capi::FixedDecimalRoundingIncrement_MultiplesOf5:
    case icu4x::capi::FixedDecimalRoundingIncrement_MultiplesOf25:
      return static_cast<icu4x::FixedDecimalRoundingIncrement::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_FixedDecimalRoundingIncrement_HPP
