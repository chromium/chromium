#ifndef ICU4X_LineBreak_D_HPP
#define ICU4X_LineBreak_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
class LineBreak;
} // namespace icu4x



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
      LineBreak_UnambiguousHyphen = 48,
    };

    typedef struct LineBreak_option {union { LineBreak ok; }; bool is_ok; } LineBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `LineBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html) for more information.
 */
class LineBreak {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Unknown`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Unknown) for more information.
         */
        Unknown = 0,
        /**
         * See the [Rust documentation for `Ambiguous`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Ambiguous) for more information.
         */
        Ambiguous = 1,
        /**
         * See the [Rust documentation for `Alphabetic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Alphabetic) for more information.
         */
        Alphabetic = 2,
        /**
         * See the [Rust documentation for `BreakBoth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.BreakBoth) for more information.
         */
        BreakBoth = 3,
        /**
         * See the [Rust documentation for `BreakAfter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.BreakAfter) for more information.
         */
        BreakAfter = 4,
        /**
         * See the [Rust documentation for `BreakBefore`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.BreakBefore) for more information.
         */
        BreakBefore = 5,
        /**
         * See the [Rust documentation for `MandatoryBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.MandatoryBreak) for more information.
         */
        MandatoryBreak = 6,
        /**
         * See the [Rust documentation for `ContingentBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.ContingentBreak) for more information.
         */
        ContingentBreak = 7,
        /**
         * See the [Rust documentation for `ClosePunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.ClosePunctuation) for more information.
         */
        ClosePunctuation = 8,
        /**
         * See the [Rust documentation for `CombiningMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.CombiningMark) for more information.
         */
        CombiningMark = 9,
        /**
         * See the [Rust documentation for `CarriageReturn`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.CarriageReturn) for more information.
         */
        CarriageReturn = 10,
        /**
         * See the [Rust documentation for `Exclamation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Exclamation) for more information.
         */
        Exclamation = 11,
        /**
         * See the [Rust documentation for `Glue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Glue) for more information.
         */
        Glue = 12,
        /**
         * See the [Rust documentation for `Hyphen`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Hyphen) for more information.
         */
        Hyphen = 13,
        /**
         * See the [Rust documentation for `Ideographic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Ideographic) for more information.
         */
        Ideographic = 14,
        /**
         * See the [Rust documentation for `Inseparable`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Inseparable) for more information.
         */
        Inseparable = 15,
        /**
         * See the [Rust documentation for `InfixNumeric`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.InfixNumeric) for more information.
         */
        InfixNumeric = 16,
        /**
         * See the [Rust documentation for `LineFeed`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.LineFeed) for more information.
         */
        LineFeed = 17,
        /**
         * See the [Rust documentation for `Nonstarter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Nonstarter) for more information.
         */
        Nonstarter = 18,
        /**
         * See the [Rust documentation for `Numeric`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Numeric) for more information.
         */
        Numeric = 19,
        /**
         * See the [Rust documentation for `OpenPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.OpenPunctuation) for more information.
         */
        OpenPunctuation = 20,
        /**
         * See the [Rust documentation for `PostfixNumeric`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.PostfixNumeric) for more information.
         */
        PostfixNumeric = 21,
        /**
         * See the [Rust documentation for `PrefixNumeric`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.PrefixNumeric) for more information.
         */
        PrefixNumeric = 22,
        /**
         * See the [Rust documentation for `Quotation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Quotation) for more information.
         */
        Quotation = 23,
        /**
         * See the [Rust documentation for `ComplexContext`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.ComplexContext) for more information.
         */
        ComplexContext = 24,
        /**
         * See the [Rust documentation for `Surrogate`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Surrogate) for more information.
         */
        Surrogate = 25,
        /**
         * See the [Rust documentation for `Space`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Space) for more information.
         */
        Space = 26,
        /**
         * See the [Rust documentation for `BreakSymbols`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.BreakSymbols) for more information.
         */
        BreakSymbols = 27,
        /**
         * See the [Rust documentation for `ZWSpace`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.ZWSpace) for more information.
         */
        ZWSpace = 28,
        /**
         * See the [Rust documentation for `NextLine`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.NextLine) for more information.
         */
        NextLine = 29,
        /**
         * See the [Rust documentation for `WordJoiner`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.WordJoiner) for more information.
         */
        WordJoiner = 30,
        /**
         * See the [Rust documentation for `H2`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.H2) for more information.
         */
        H2 = 31,
        /**
         * See the [Rust documentation for `H3`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.H3) for more information.
         */
        H3 = 32,
        /**
         * See the [Rust documentation for `JL`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.JL) for more information.
         */
        JL = 33,
        /**
         * See the [Rust documentation for `JT`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.JT) for more information.
         */
        JT = 34,
        /**
         * See the [Rust documentation for `JV`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.JV) for more information.
         */
        JV = 35,
        /**
         * See the [Rust documentation for `CloseParenthesis`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.CloseParenthesis) for more information.
         */
        CloseParenthesis = 36,
        /**
         * See the [Rust documentation for `ConditionalJapaneseStarter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.ConditionalJapaneseStarter) for more information.
         */
        ConditionalJapaneseStarter = 37,
        /**
         * See the [Rust documentation for `HebrewLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.HebrewLetter) for more information.
         */
        HebrewLetter = 38,
        /**
         * See the [Rust documentation for `RegionalIndicator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.RegionalIndicator) for more information.
         */
        RegionalIndicator = 39,
        /**
         * See the [Rust documentation for `EBase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.EBase) for more information.
         */
        EBase = 40,
        /**
         * See the [Rust documentation for `EModifier`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.EModifier) for more information.
         */
        EModifier = 41,
        /**
         * See the [Rust documentation for `ZWJ`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.ZWJ) for more information.
         */
        ZWJ = 42,
        /**
         * See the [Rust documentation for `Aksara`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Aksara) for more information.
         */
        Aksara = 43,
        /**
         * See the [Rust documentation for `AksaraPrebase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.AksaraPrebase) for more information.
         */
        AksaraPrebase = 44,
        /**
         * See the [Rust documentation for `AksaraStart`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.AksaraStart) for more information.
         */
        AksaraStart = 45,
        /**
         * See the [Rust documentation for `ViramaFinal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.ViramaFinal) for more information.
         */
        ViramaFinal = 46,
        /**
         * See the [Rust documentation for `Virama`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.Virama) for more information.
         */
        Virama = 47,
        /**
         * See the [Rust documentation for `UnambiguousHyphen`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#associatedconstant.UnambiguousHyphen) for more information.
         */
        UnambiguousHyphen = 48,
    };

    LineBreak(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr LineBreak(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::LineBreak for_char(char32_t ch);

  /**
   * Get the "long" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyNamesLongBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> long_name() const;

  /**
   * Get the "short" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyNamesShortBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> short_name() const;

  /**
   * Convert to an integer value usable with ICU4C and `CodePointMapData`
   *
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::LineBreak> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::LineBreak> try_from_str(std::string_view s);

    inline icu4x::capi::LineBreak AsFFI() const;
    inline static icu4x::LineBreak FromFFI(icu4x::capi::LineBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_LineBreak_D_HPP
