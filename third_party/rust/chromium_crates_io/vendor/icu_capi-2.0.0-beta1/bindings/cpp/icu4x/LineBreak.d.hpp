#ifndef icu4x_LineBreak_D_HPP
#define icu4x_LineBreak_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class LineBreak;
}


namespace icu4x {
namespace capi {
    enum LineBreak {
      LineBreak_Unknown = 0,
      LineBreak_Ambiguous = 1,
      LineBreak_Alphabetic = 2,
      LineBreak_BreakBoth = 3,
      LineBreak_BreakAfter = 4,
      LineBreak_BreakBefore = 5,
      LineBreak_MandatoryBreak = 6,
      LineBreak_ContingentBreak = 7,
      LineBreak_ClosePunctuation = 8,
      LineBreak_CombiningMark = 9,
      LineBreak_CarriageReturn = 10,
      LineBreak_Exclamation = 11,
      LineBreak_Glue = 12,
      LineBreak_Hyphen = 13,
      LineBreak_Ideographic = 14,
      LineBreak_Inseparable = 15,
      LineBreak_InfixNumeric = 16,
      LineBreak_LineFeed = 17,
      LineBreak_Nonstarter = 18,
      LineBreak_Numeric = 19,
      LineBreak_OpenPunctuation = 20,
      LineBreak_PostfixNumeric = 21,
      LineBreak_PrefixNumeric = 22,
      LineBreak_Quotation = 23,
      LineBreak_ComplexContext = 24,
      LineBreak_Surrogate = 25,
      LineBreak_Space = 26,
      LineBreak_BreakSymbols = 27,
      LineBreak_ZWSpace = 28,
      LineBreak_NextLine = 29,
      LineBreak_WordJoiner = 30,
      LineBreak_H2 = 31,
      LineBreak_H3 = 32,
      LineBreak_JL = 33,
      LineBreak_JT = 34,
      LineBreak_JV = 35,
      LineBreak_CloseParenthesis = 36,
      LineBreak_ConditionalJapaneseStarter = 37,
      LineBreak_HebrewLetter = 38,
      LineBreak_RegionalIndicator = 39,
      LineBreak_EBase = 40,
      LineBreak_EModifier = 41,
      LineBreak_ZWJ = 42,
      LineBreak_Aksara = 43,
      LineBreak_AksaraPrebase = 44,
      LineBreak_AksaraStart = 45,
      LineBreak_ViramaFinal = 46,
      LineBreak_Virama = 47,
    };
    
    typedef struct LineBreak_option {union { LineBreak ok; }; bool is_ok; } LineBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
class LineBreak {
public:
  enum Value {
    Unknown = 0,
    Ambiguous = 1,
    Alphabetic = 2,
    BreakBoth = 3,
    BreakAfter = 4,
    BreakBefore = 5,
    MandatoryBreak = 6,
    ContingentBreak = 7,
    ClosePunctuation = 8,
    CombiningMark = 9,
    CarriageReturn = 10,
    Exclamation = 11,
    Glue = 12,
    Hyphen = 13,
    Ideographic = 14,
    Inseparable = 15,
    InfixNumeric = 16,
    LineFeed = 17,
    Nonstarter = 18,
    Numeric = 19,
    OpenPunctuation = 20,
    PostfixNumeric = 21,
    PrefixNumeric = 22,
    Quotation = 23,
    ComplexContext = 24,
    Surrogate = 25,
    Space = 26,
    BreakSymbols = 27,
    ZWSpace = 28,
    NextLine = 29,
    WordJoiner = 30,
    H2 = 31,
    H3 = 32,
    JL = 33,
    JT = 34,
    JV = 35,
    CloseParenthesis = 36,
    ConditionalJapaneseStarter = 37,
    HebrewLetter = 38,
    RegionalIndicator = 39,
    EBase = 40,
    EModifier = 41,
    ZWJ = 42,
    Aksara = 43,
    AksaraPrebase = 44,
    AksaraStart = 45,
    ViramaFinal = 46,
    Virama = 47,
  };

  LineBreak() = default;
  // Implicit conversions between enum and ::Value
  constexpr LineBreak(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline uint8_t to_integer();

  inline static std::optional<icu4x::LineBreak> from_integer(uint8_t other);

  inline icu4x::capi::LineBreak AsFFI() const;
  inline static icu4x::LineBreak FromFFI(icu4x::capi::LineBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LineBreak_D_HPP
