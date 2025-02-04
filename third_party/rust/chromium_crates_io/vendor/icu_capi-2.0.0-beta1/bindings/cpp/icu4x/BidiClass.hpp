#ifndef icu4x_BidiClass_HPP
#define icu4x_BidiClass_HPP

#include "BidiClass.d.hpp"

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
    
    uint8_t icu4x_BidiClass_to_integer_mv1(icu4x::capi::BidiClass self);
    
    typedef struct icu4x_BidiClass_from_integer_mv1_result {union {icu4x::capi::BidiClass ok; }; bool is_ok;} icu4x_BidiClass_from_integer_mv1_result;
    icu4x_BidiClass_from_integer_mv1_result icu4x_BidiClass_from_integer_mv1(uint8_t other);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::BidiClass icu4x::BidiClass::AsFFI() const {
  return static_cast<icu4x::capi::BidiClass>(value);
}

inline icu4x::BidiClass icu4x::BidiClass::FromFFI(icu4x::capi::BidiClass c_enum) {
  switch (c_enum) {
    case icu4x::capi::BidiClass_LeftToRight:
    case icu4x::capi::BidiClass_RightToLeft:
    case icu4x::capi::BidiClass_EuropeanNumber:
    case icu4x::capi::BidiClass_EuropeanSeparator:
    case icu4x::capi::BidiClass_EuropeanTerminator:
    case icu4x::capi::BidiClass_ArabicNumber:
    case icu4x::capi::BidiClass_CommonSeparator:
    case icu4x::capi::BidiClass_ParagraphSeparator:
    case icu4x::capi::BidiClass_SegmentSeparator:
    case icu4x::capi::BidiClass_WhiteSpace:
    case icu4x::capi::BidiClass_OtherNeutral:
    case icu4x::capi::BidiClass_LeftToRightEmbedding:
    case icu4x::capi::BidiClass_LeftToRightOverride:
    case icu4x::capi::BidiClass_ArabicLetter:
    case icu4x::capi::BidiClass_RightToLeftEmbedding:
    case icu4x::capi::BidiClass_RightToLeftOverride:
    case icu4x::capi::BidiClass_PopDirectionalFormat:
    case icu4x::capi::BidiClass_NonspacingMark:
    case icu4x::capi::BidiClass_BoundaryNeutral:
    case icu4x::capi::BidiClass_FirstStrongIsolate:
    case icu4x::capi::BidiClass_LeftToRightIsolate:
    case icu4x::capi::BidiClass_RightToLeftIsolate:
    case icu4x::capi::BidiClass_PopDirectionalIsolate:
      return static_cast<icu4x::BidiClass::Value>(c_enum);
    default:
      abort();
  }
}

inline uint8_t icu4x::BidiClass::to_integer() {
  auto result = icu4x::capi::icu4x_BidiClass_to_integer_mv1(this->AsFFI());
  return result;
}

inline std::optional<icu4x::BidiClass> icu4x::BidiClass::from_integer(uint8_t other) {
  auto result = icu4x::capi::icu4x_BidiClass_from_integer_mv1(other);
  return result.is_ok ? std::optional<icu4x::BidiClass>(icu4x::BidiClass::FromFFI(result.ok)) : std::nullopt;
}
#endif // icu4x_BidiClass_HPP
