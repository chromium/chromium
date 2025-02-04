#ifndef icu4x_LineBreak_HPP
#define icu4x_LineBreak_HPP

#include "LineBreak.d.hpp"

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
    
    uint8_t icu4x_LineBreak_to_integer_mv1(icu4x::capi::LineBreak self);
    
    typedef struct icu4x_LineBreak_from_integer_mv1_result {union {icu4x::capi::LineBreak ok; }; bool is_ok;} icu4x_LineBreak_from_integer_mv1_result;
    icu4x_LineBreak_from_integer_mv1_result icu4x_LineBreak_from_integer_mv1(uint8_t other);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::LineBreak icu4x::LineBreak::AsFFI() const {
  return static_cast<icu4x::capi::LineBreak>(value);
}

inline icu4x::LineBreak icu4x::LineBreak::FromFFI(icu4x::capi::LineBreak c_enum) {
  switch (c_enum) {
    case icu4x::capi::LineBreak_Unknown:
    case icu4x::capi::LineBreak_Ambiguous:
    case icu4x::capi::LineBreak_Alphabetic:
    case icu4x::capi::LineBreak_BreakBoth:
    case icu4x::capi::LineBreak_BreakAfter:
    case icu4x::capi::LineBreak_BreakBefore:
    case icu4x::capi::LineBreak_MandatoryBreak:
    case icu4x::capi::LineBreak_ContingentBreak:
    case icu4x::capi::LineBreak_ClosePunctuation:
    case icu4x::capi::LineBreak_CombiningMark:
    case icu4x::capi::LineBreak_CarriageReturn:
    case icu4x::capi::LineBreak_Exclamation:
    case icu4x::capi::LineBreak_Glue:
    case icu4x::capi::LineBreak_Hyphen:
    case icu4x::capi::LineBreak_Ideographic:
    case icu4x::capi::LineBreak_Inseparable:
    case icu4x::capi::LineBreak_InfixNumeric:
    case icu4x::capi::LineBreak_LineFeed:
    case icu4x::capi::LineBreak_Nonstarter:
    case icu4x::capi::LineBreak_Numeric:
    case icu4x::capi::LineBreak_OpenPunctuation:
    case icu4x::capi::LineBreak_PostfixNumeric:
    case icu4x::capi::LineBreak_PrefixNumeric:
    case icu4x::capi::LineBreak_Quotation:
    case icu4x::capi::LineBreak_ComplexContext:
    case icu4x::capi::LineBreak_Surrogate:
    case icu4x::capi::LineBreak_Space:
    case icu4x::capi::LineBreak_BreakSymbols:
    case icu4x::capi::LineBreak_ZWSpace:
    case icu4x::capi::LineBreak_NextLine:
    case icu4x::capi::LineBreak_WordJoiner:
    case icu4x::capi::LineBreak_H2:
    case icu4x::capi::LineBreak_H3:
    case icu4x::capi::LineBreak_JL:
    case icu4x::capi::LineBreak_JT:
    case icu4x::capi::LineBreak_JV:
    case icu4x::capi::LineBreak_CloseParenthesis:
    case icu4x::capi::LineBreak_ConditionalJapaneseStarter:
    case icu4x::capi::LineBreak_HebrewLetter:
    case icu4x::capi::LineBreak_RegionalIndicator:
    case icu4x::capi::LineBreak_EBase:
    case icu4x::capi::LineBreak_EModifier:
    case icu4x::capi::LineBreak_ZWJ:
    case icu4x::capi::LineBreak_Aksara:
    case icu4x::capi::LineBreak_AksaraPrebase:
    case icu4x::capi::LineBreak_AksaraStart:
    case icu4x::capi::LineBreak_ViramaFinal:
    case icu4x::capi::LineBreak_Virama:
      return static_cast<icu4x::LineBreak::Value>(c_enum);
    default:
      abort();
  }
}

inline uint8_t icu4x::LineBreak::to_integer() {
  auto result = icu4x::capi::icu4x_LineBreak_to_integer_mv1(this->AsFFI());
  return result;
}

inline std::optional<icu4x::LineBreak> icu4x::LineBreak::from_integer(uint8_t other) {
  auto result = icu4x::capi::icu4x_LineBreak_from_integer_mv1(other);
  return result.is_ok ? std::optional<icu4x::LineBreak>(icu4x::LineBreak::FromFFI(result.ok)) : std::nullopt;
}
#endif // icu4x_LineBreak_HPP
