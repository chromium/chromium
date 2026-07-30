#ifndef ICU4X_SentenceBreak_D_HPP
#define ICU4X_SentenceBreak_D_HPP

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
class SentenceBreak;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum SentenceBreak {
      SentenceBreak_Other = 0,
      SentenceBreak_ATerm = 1,
      SentenceBreak_Close = 2,
      SentenceBreak_Format = 3,
      SentenceBreak_Lower = 4,
      SentenceBreak_Numeric = 5,
      SentenceBreak_OLetter = 6,
      SentenceBreak_Sep = 7,
      SentenceBreak_Sp = 8,
      SentenceBreak_STerm = 9,
      SentenceBreak_Upper = 10,
      SentenceBreak_CR = 11,
      SentenceBreak_Extend = 12,
      SentenceBreak_LF = 13,
      SentenceBreak_SContinue = 14,
    };

    typedef struct SentenceBreak_option {union { SentenceBreak ok; }; bool is_ok; } SentenceBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `SentenceBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html) for more information.
 */
class SentenceBreak {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Other`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Other) for more information.
         */
        Other = 0,
        /**
         * See the [Rust documentation for `ATerm`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.ATerm) for more information.
         */
        ATerm = 1,
        /**
         * See the [Rust documentation for `Close`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Close) for more information.
         */
        Close = 2,
        /**
         * See the [Rust documentation for `Format`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Format) for more information.
         */
        Format = 3,
        /**
         * See the [Rust documentation for `Lower`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Lower) for more information.
         */
        Lower = 4,
        /**
         * See the [Rust documentation for `Numeric`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Numeric) for more information.
         */
        Numeric = 5,
        /**
         * See the [Rust documentation for `OLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.OLetter) for more information.
         */
        OLetter = 6,
        /**
         * See the [Rust documentation for `Sep`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Sep) for more information.
         */
        Sep = 7,
        /**
         * See the [Rust documentation for `Sp`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Sp) for more information.
         */
        Sp = 8,
        /**
         * See the [Rust documentation for `STerm`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.STerm) for more information.
         */
        STerm = 9,
        /**
         * See the [Rust documentation for `Upper`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Upper) for more information.
         */
        Upper = 10,
        /**
         * See the [Rust documentation for `CR`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.CR) for more information.
         */
        CR = 11,
        /**
         * See the [Rust documentation for `Extend`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.Extend) for more information.
         */
        Extend = 12,
        /**
         * See the [Rust documentation for `LF`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.LF) for more information.
         */
        LF = 13,
        /**
         * See the [Rust documentation for `SContinue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#associatedconstant.SContinue) for more information.
         */
        SContinue = 14,
    };

    SentenceBreak(): value(Value::Other) {}

    // Implicit conversions between enum and ::Value
    constexpr SentenceBreak(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::SentenceBreak for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::SentenceBreak> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::SentenceBreak> try_from_str(std::string_view s);

    inline icu4x::capi::SentenceBreak AsFFI() const;
    inline static icu4x::SentenceBreak FromFFI(icu4x::capi::SentenceBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_SentenceBreak_D_HPP
