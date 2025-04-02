#ifndef icu4x_GraphemeClusterBreak_HPP
#define icu4x_GraphemeClusterBreak_HPP

#include "GraphemeClusterBreak.d.hpp"

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
    
    icu4x::capi::GraphemeClusterBreak icu4x_GraphemeClusterBreak_for_char_mv1(char32_t ch);
    
    uint8_t icu4x_GraphemeClusterBreak_to_integer_value_mv1(icu4x::capi::GraphemeClusterBreak self);
    
    typedef struct icu4x_GraphemeClusterBreak_from_integer_value_mv1_result {union {icu4x::capi::GraphemeClusterBreak ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_from_integer_value_mv1_result;
    icu4x_GraphemeClusterBreak_from_integer_value_mv1_result icu4x_GraphemeClusterBreak_from_integer_value_mv1(uint8_t other);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::GraphemeClusterBreak icu4x::GraphemeClusterBreak::AsFFI() const {
  return static_cast<icu4x::capi::GraphemeClusterBreak>(value);
}

inline icu4x::GraphemeClusterBreak icu4x::GraphemeClusterBreak::FromFFI(icu4x::capi::GraphemeClusterBreak c_enum) {
  switch (c_enum) {
    case icu4x::capi::GraphemeClusterBreak_Other:
    case icu4x::capi::GraphemeClusterBreak_Control:
    case icu4x::capi::GraphemeClusterBreak_CR:
    case icu4x::capi::GraphemeClusterBreak_Extend:
    case icu4x::capi::GraphemeClusterBreak_L:
    case icu4x::capi::GraphemeClusterBreak_LF:
    case icu4x::capi::GraphemeClusterBreak_LV:
    case icu4x::capi::GraphemeClusterBreak_LVT:
    case icu4x::capi::GraphemeClusterBreak_T:
    case icu4x::capi::GraphemeClusterBreak_V:
    case icu4x::capi::GraphemeClusterBreak_SpacingMark:
    case icu4x::capi::GraphemeClusterBreak_Prepend:
    case icu4x::capi::GraphemeClusterBreak_RegionalIndicator:
    case icu4x::capi::GraphemeClusterBreak_EBase:
    case icu4x::capi::GraphemeClusterBreak_EBaseGAZ:
    case icu4x::capi::GraphemeClusterBreak_EModifier:
    case icu4x::capi::GraphemeClusterBreak_GlueAfterZwj:
    case icu4x::capi::GraphemeClusterBreak_ZWJ:
      return static_cast<icu4x::GraphemeClusterBreak::Value>(c_enum);
    default:
      abort();
  }
}

inline icu4x::GraphemeClusterBreak icu4x::GraphemeClusterBreak::for_char(char32_t ch) {
  auto result = icu4x::capi::icu4x_GraphemeClusterBreak_for_char_mv1(ch);
  return icu4x::GraphemeClusterBreak::FromFFI(result);
}

inline uint8_t icu4x::GraphemeClusterBreak::to_integer_value() {
  auto result = icu4x::capi::icu4x_GraphemeClusterBreak_to_integer_value_mv1(this->AsFFI());
  return result;
}

inline std::optional<icu4x::GraphemeClusterBreak> icu4x::GraphemeClusterBreak::from_integer_value(uint8_t other) {
  auto result = icu4x::capi::icu4x_GraphemeClusterBreak_from_integer_value_mv1(other);
  return result.is_ok ? std::optional<icu4x::GraphemeClusterBreak>(icu4x::GraphemeClusterBreak::FromFFI(result.ok)) : std::nullopt;
}
#endif // icu4x_GraphemeClusterBreak_HPP
