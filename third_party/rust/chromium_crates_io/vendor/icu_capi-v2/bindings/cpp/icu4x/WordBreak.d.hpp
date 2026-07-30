#ifndef ICU4X_WordBreak_D_HPP
#define ICU4X_WordBreak_D_HPP

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
class WordBreak;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum WordBreak {
      WordBreak_Other = 0,
      WordBreak_ALetter = 1,
      WordBreak_Format = 2,
      WordBreak_Katakana = 3,
      WordBreak_MidLetter = 4,
      WordBreak_MidNum = 5,
      WordBreak_Numeric = 6,
      WordBreak_ExtendNumLet = 7,
      WordBreak_CR = 8,
      WordBreak_Extend = 9,
      WordBreak_LF = 10,
      WordBreak_MidNumLet = 11,
      WordBreak_Newline = 12,
      WordBreak_RegionalIndicator = 13,
      WordBreak_HebrewLetter = 14,
      WordBreak_SingleQuote = 15,
      WordBreak_DoubleQuote = 16,
      WordBreak_EBase = 17,
      WordBreak_EBaseGAZ = 18,
      WordBreak_EModifier = 19,
      WordBreak_GlueAfterZwj = 20,
      WordBreak_ZWJ = 21,
      WordBreak_WSegSpace = 22,
    };

    typedef struct WordBreak_option {union { WordBreak ok; }; bool is_ok; } WordBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `WordBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html) for more information.
 */
class WordBreak {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Other`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.Other) for more information.
         */
        Other = 0,
        /**
         * See the [Rust documentation for `ALetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.ALetter) for more information.
         */
        ALetter = 1,
        /**
         * See the [Rust documentation for `Format`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.Format) for more information.
         */
        Format = 2,
        /**
         * See the [Rust documentation for `Katakana`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.Katakana) for more information.
         */
        Katakana = 3,
        /**
         * See the [Rust documentation for `MidLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.MidLetter) for more information.
         */
        MidLetter = 4,
        /**
         * See the [Rust documentation for `MidNum`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.MidNum) for more information.
         */
        MidNum = 5,
        /**
         * See the [Rust documentation for `Numeric`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.Numeric) for more information.
         */
        Numeric = 6,
        /**
         * See the [Rust documentation for `ExtendNumLet`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.ExtendNumLet) for more information.
         */
        ExtendNumLet = 7,
        /**
         * See the [Rust documentation for `CR`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.CR) for more information.
         */
        CR = 8,
        /**
         * See the [Rust documentation for `Extend`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.Extend) for more information.
         */
        Extend = 9,
        /**
         * See the [Rust documentation for `LF`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.LF) for more information.
         */
        LF = 10,
        /**
         * See the [Rust documentation for `MidNumLet`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.MidNumLet) for more information.
         */
        MidNumLet = 11,
        /**
         * See the [Rust documentation for `Newline`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.Newline) for more information.
         */
        Newline = 12,
        /**
         * See the [Rust documentation for `RegionalIndicator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.RegionalIndicator) for more information.
         */
        RegionalIndicator = 13,
        /**
         * See the [Rust documentation for `HebrewLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.HebrewLetter) for more information.
         */
        HebrewLetter = 14,
        /**
         * See the [Rust documentation for `SingleQuote`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.SingleQuote) for more information.
         */
        SingleQuote = 15,
        /**
         * See the [Rust documentation for `DoubleQuote`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.DoubleQuote) for more information.
         */
        DoubleQuote = 16,
        /**
         * See the [Rust documentation for `EBase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.EBase) for more information.
         */
        EBase = 17,
        /**
         * See the [Rust documentation for `EBaseGAZ`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.EBaseGAZ) for more information.
         */
        EBaseGAZ = 18,
        /**
         * See the [Rust documentation for `EModifier`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.EModifier) for more information.
         */
        EModifier = 19,
        /**
         * See the [Rust documentation for `GlueAfterZwj`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.GlueAfterZwj) for more information.
         */
        GlueAfterZwj = 20,
        /**
         * See the [Rust documentation for `ZWJ`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.ZWJ) for more information.
         */
        ZWJ = 21,
        /**
         * See the [Rust documentation for `WSegSpace`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#associatedconstant.WSegSpace) for more information.
         */
        WSegSpace = 22,
    };

    WordBreak(): value(Value::Other) {}

    // Implicit conversions between enum and ::Value
    constexpr WordBreak(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::WordBreak for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::WordBreak> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::WordBreak> try_from_str(std::string_view s);

    inline icu4x::capi::WordBreak AsFFI() const;
    inline static icu4x::WordBreak FromFFI(icu4x::capi::WordBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_WordBreak_D_HPP
