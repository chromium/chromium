#ifndef ICU4X_HangulSyllableType_D_HPP
#define ICU4X_HangulSyllableType_D_HPP

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
class HangulSyllableType;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum HangulSyllableType {
      HangulSyllableType_NotApplicable = 0,
      HangulSyllableType_LeadingJamo = 1,
      HangulSyllableType_VowelJamo = 2,
      HangulSyllableType_TrailingJamo = 3,
      HangulSyllableType_LeadingVowelSyllable = 4,
      HangulSyllableType_LeadingVowelTrailingSyllable = 5,
    };

    typedef struct HangulSyllableType_option {union { HangulSyllableType ok; }; bool is_ok; } HangulSyllableType_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `HangulSyllableType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html) for more information.
 */
class HangulSyllableType {
public:
    enum Value {
        /**
         * See the [Rust documentation for `NotApplicable`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#associatedconstant.NotApplicable) for more information.
         */
        NotApplicable = 0,
        /**
         * See the [Rust documentation for `LeadingJamo`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#associatedconstant.LeadingJamo) for more information.
         */
        LeadingJamo = 1,
        /**
         * See the [Rust documentation for `VowelJamo`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#associatedconstant.VowelJamo) for more information.
         */
        VowelJamo = 2,
        /**
         * See the [Rust documentation for `TrailingJamo`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#associatedconstant.TrailingJamo) for more information.
         */
        TrailingJamo = 3,
        /**
         * See the [Rust documentation for `LeadingVowelSyllable`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#associatedconstant.LeadingVowelSyllable) for more information.
         */
        LeadingVowelSyllable = 4,
        /**
         * See the [Rust documentation for `LeadingVowelTrailingSyllable`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#associatedconstant.LeadingVowelTrailingSyllable) for more information.
         */
        LeadingVowelTrailingSyllable = 5,
    };

    HangulSyllableType(): value(Value::NotApplicable) {}

    // Implicit conversions between enum and ::Value
    constexpr HangulSyllableType(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::HangulSyllableType for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::HangulSyllableType> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::HangulSyllableType> try_from_str(std::string_view s);

    inline icu4x::capi::HangulSyllableType AsFFI() const;
    inline static icu4x::HangulSyllableType FromFFI(icu4x::capi::HangulSyllableType c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_HangulSyllableType_D_HPP
