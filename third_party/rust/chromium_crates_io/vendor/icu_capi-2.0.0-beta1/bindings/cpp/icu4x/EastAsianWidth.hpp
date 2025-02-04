#ifndef icu4x_EastAsianWidth_HPP
#define icu4x_EastAsianWidth_HPP

#include "EastAsianWidth.d.hpp"

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
    
    uint8_t icu4x_EastAsianWidth_to_integer_mv1(icu4x::capi::EastAsianWidth self);
    
    typedef struct icu4x_EastAsianWidth_from_integer_mv1_result {union {icu4x::capi::EastAsianWidth ok; }; bool is_ok;} icu4x_EastAsianWidth_from_integer_mv1_result;
    icu4x_EastAsianWidth_from_integer_mv1_result icu4x_EastAsianWidth_from_integer_mv1(uint8_t other);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::EastAsianWidth icu4x::EastAsianWidth::AsFFI() const {
  return static_cast<icu4x::capi::EastAsianWidth>(value);
}

inline icu4x::EastAsianWidth icu4x::EastAsianWidth::FromFFI(icu4x::capi::EastAsianWidth c_enum) {
  switch (c_enum) {
    case icu4x::capi::EastAsianWidth_Neutral:
    case icu4x::capi::EastAsianWidth_Ambiguous:
    case icu4x::capi::EastAsianWidth_Halfwidth:
    case icu4x::capi::EastAsianWidth_Fullwidth:
    case icu4x::capi::EastAsianWidth_Narrow:
    case icu4x::capi::EastAsianWidth_Wide:
      return static_cast<icu4x::EastAsianWidth::Value>(c_enum);
    default:
      abort();
  }
}

inline uint8_t icu4x::EastAsianWidth::to_integer() {
  auto result = icu4x::capi::icu4x_EastAsianWidth_to_integer_mv1(this->AsFFI());
  return result;
}

inline std::optional<icu4x::EastAsianWidth> icu4x::EastAsianWidth::from_integer(uint8_t other) {
  auto result = icu4x::capi::icu4x_EastAsianWidth_from_integer_mv1(other);
  return result.is_ok ? std::optional<icu4x::EastAsianWidth>(icu4x::EastAsianWidth::FromFFI(result.ok)) : std::nullopt;
}
#endif // icu4x_EastAsianWidth_HPP
