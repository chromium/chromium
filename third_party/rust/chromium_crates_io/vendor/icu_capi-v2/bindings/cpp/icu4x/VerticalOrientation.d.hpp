#ifndef ICU4X_VerticalOrientation_D_HPP
#define ICU4X_VerticalOrientation_D_HPP

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
class VerticalOrientation;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum VerticalOrientation {
      VerticalOrientation_Rotated = 0,
      VerticalOrientation_TransformedRotated = 1,
      VerticalOrientation_TransformedUpright = 2,
      VerticalOrientation_Upright = 3,
    };

    typedef struct VerticalOrientation_option {union { VerticalOrientation ok; }; bool is_ok; } VerticalOrientation_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `VerticalOrientation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html) for more information.
 */
class VerticalOrientation {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Rotated`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html#associatedconstant.Rotated) for more information.
         */
        Rotated = 0,
        /**
         * See the [Rust documentation for `TransformedRotated`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html#associatedconstant.TransformedRotated) for more information.
         */
        TransformedRotated = 1,
        /**
         * See the [Rust documentation for `TransformedUpright`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html#associatedconstant.TransformedUpright) for more information.
         */
        TransformedUpright = 2,
        /**
         * See the [Rust documentation for `Upright`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html#associatedconstant.Upright) for more information.
         */
        Upright = 3,
    };

    VerticalOrientation(): value(Value::Rotated) {}

    // Implicit conversions between enum and ::Value
    constexpr VerticalOrientation(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::VerticalOrientation for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::VerticalOrientation> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::VerticalOrientation> try_from_str(std::string_view s);

    inline icu4x::capi::VerticalOrientation AsFFI() const;
    inline static icu4x::VerticalOrientation FromFFI(icu4x::capi::VerticalOrientation c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_VerticalOrientation_D_HPP
