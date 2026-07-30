#ifndef ICU4X_NumericType_D_HPP
#define ICU4X_NumericType_D_HPP

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
class NumericType;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum NumericType {
      NumericType_None = 0,
      NumericType_Decimal = 1,
      NumericType_Digit = 2,
      NumericType_Numeric = 3,
    };

    typedef struct NumericType_option {union { NumericType ok; }; bool is_ok; } NumericType_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `NumericType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html) for more information.
 */
class NumericType {
public:
    enum Value {
        /**
         * See the [Rust documentation for `None`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html#associatedconstant.None) for more information.
         */
        None = 0,
        /**
         * See the [Rust documentation for `Decimal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html#associatedconstant.Decimal) for more information.
         */
        Decimal = 1,
        /**
         * See the [Rust documentation for `Digit`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html#associatedconstant.Digit) for more information.
         */
        Digit = 2,
        /**
         * See the [Rust documentation for `Numeric`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html#associatedconstant.Numeric) for more information.
         */
        Numeric = 3,
    };

    NumericType(): value(Value::None) {}

    // Implicit conversions between enum and ::Value
    constexpr NumericType(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::NumericType for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::NumericType> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::NumericType> try_from_str(std::string_view s);

    inline icu4x::capi::NumericType AsFFI() const;
    inline static icu4x::NumericType FromFFI(icu4x::capi::NumericType c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_NumericType_D_HPP
