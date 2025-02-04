#ifndef icu4x_HangulSyllableType_HPP
#define icu4x_HangulSyllableType_HPP

#include "HangulSyllableType.d.hpp"

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
    
    uint8_t icu4x_HangulSyllableType_to_integer_mv1(icu4x::capi::HangulSyllableType self);
    
    typedef struct icu4x_HangulSyllableType_from_integer_mv1_result {union {icu4x::capi::HangulSyllableType ok; }; bool is_ok;} icu4x_HangulSyllableType_from_integer_mv1_result;
    icu4x_HangulSyllableType_from_integer_mv1_result icu4x_HangulSyllableType_from_integer_mv1(uint8_t other);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::HangulSyllableType icu4x::HangulSyllableType::AsFFI() const {
  return static_cast<icu4x::capi::HangulSyllableType>(value);
}

inline icu4x::HangulSyllableType icu4x::HangulSyllableType::FromFFI(icu4x::capi::HangulSyllableType c_enum) {
  switch (c_enum) {
    case icu4x::capi::HangulSyllableType_NotApplicable:
    case icu4x::capi::HangulSyllableType_LeadingJamo:
    case icu4x::capi::HangulSyllableType_VowelJamo:
    case icu4x::capi::HangulSyllableType_TrailingJamo:
    case icu4x::capi::HangulSyllableType_LeadingVowelSyllable:
    case icu4x::capi::HangulSyllableType_LeadingVowelTrailingSyllable:
      return static_cast<icu4x::HangulSyllableType::Value>(c_enum);
    default:
      abort();
  }
}

inline uint8_t icu4x::HangulSyllableType::to_integer() {
  auto result = icu4x::capi::icu4x_HangulSyllableType_to_integer_mv1(this->AsFFI());
  return result;
}

inline std::optional<icu4x::HangulSyllableType> icu4x::HangulSyllableType::from_integer(uint8_t other) {
  auto result = icu4x::capi::icu4x_HangulSyllableType_from_integer_mv1(other);
  return result.is_ok ? std::optional<icu4x::HangulSyllableType>(icu4x::HangulSyllableType::FromFFI(result.ok)) : std::nullopt;
}
#endif // icu4x_HangulSyllableType_HPP
