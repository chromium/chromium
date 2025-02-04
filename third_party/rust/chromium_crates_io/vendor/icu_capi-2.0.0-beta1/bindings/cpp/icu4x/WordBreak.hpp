#ifndef icu4x_WordBreak_HPP
#define icu4x_WordBreak_HPP

#include "WordBreak.d.hpp"

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
    
    uint8_t icu4x_WordBreak_to_integer_mv1(icu4x::capi::WordBreak self);
    
    typedef struct icu4x_WordBreak_from_integer_mv1_result {union {icu4x::capi::WordBreak ok; }; bool is_ok;} icu4x_WordBreak_from_integer_mv1_result;
    icu4x_WordBreak_from_integer_mv1_result icu4x_WordBreak_from_integer_mv1(uint8_t other);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::WordBreak icu4x::WordBreak::AsFFI() const {
  return static_cast<icu4x::capi::WordBreak>(value);
}

inline icu4x::WordBreak icu4x::WordBreak::FromFFI(icu4x::capi::WordBreak c_enum) {
  switch (c_enum) {
    case icu4x::capi::WordBreak_Other:
    case icu4x::capi::WordBreak_ALetter:
    case icu4x::capi::WordBreak_Format:
    case icu4x::capi::WordBreak_Katakana:
    case icu4x::capi::WordBreak_MidLetter:
    case icu4x::capi::WordBreak_MidNum:
    case icu4x::capi::WordBreak_Numeric:
    case icu4x::capi::WordBreak_ExtendNumLet:
    case icu4x::capi::WordBreak_CR:
    case icu4x::capi::WordBreak_Extend:
    case icu4x::capi::WordBreak_LF:
    case icu4x::capi::WordBreak_MidNumLet:
    case icu4x::capi::WordBreak_Newline:
    case icu4x::capi::WordBreak_RegionalIndicator:
    case icu4x::capi::WordBreak_HebrewLetter:
    case icu4x::capi::WordBreak_SingleQuote:
    case icu4x::capi::WordBreak_DoubleQuote:
    case icu4x::capi::WordBreak_EBase:
    case icu4x::capi::WordBreak_EBaseGAZ:
    case icu4x::capi::WordBreak_EModifier:
    case icu4x::capi::WordBreak_GlueAfterZwj:
    case icu4x::capi::WordBreak_ZWJ:
    case icu4x::capi::WordBreak_WSegSpace:
      return static_cast<icu4x::WordBreak::Value>(c_enum);
    default:
      abort();
  }
}

inline uint8_t icu4x::WordBreak::to_integer() {
  auto result = icu4x::capi::icu4x_WordBreak_to_integer_mv1(this->AsFFI());
  return result;
}

inline std::optional<icu4x::WordBreak> icu4x::WordBreak::from_integer(uint8_t other) {
  auto result = icu4x::capi::icu4x_WordBreak_from_integer_mv1(other);
  return result.is_ok ? std::optional<icu4x::WordBreak>(icu4x::WordBreak::FromFFI(result.ok)) : std::nullopt;
}
#endif // icu4x_WordBreak_HPP
