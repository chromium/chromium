#ifndef ICU4X_JoiningType_D_HPP
#define ICU4X_JoiningType_D_HPP

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
class JoiningType;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum JoiningType {
      JoiningType_NonJoining = 0,
      JoiningType_JoinCausing = 1,
      JoiningType_DualJoining = 2,
      JoiningType_LeftJoining = 3,
      JoiningType_RightJoining = 4,
      JoiningType_Transparent = 5,
    };

    typedef struct JoiningType_option {union { JoiningType ok; }; bool is_ok; } JoiningType_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `JoiningType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html) for more information.
 */
class JoiningType {
public:
    enum Value {
        /**
         * See the [Rust documentation for `NonJoining`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#associatedconstant.NonJoining) for more information.
         */
        NonJoining = 0,
        /**
         * See the [Rust documentation for `JoinCausing`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#associatedconstant.JoinCausing) for more information.
         */
        JoinCausing = 1,
        /**
         * See the [Rust documentation for `DualJoining`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#associatedconstant.DualJoining) for more information.
         */
        DualJoining = 2,
        /**
         * See the [Rust documentation for `LeftJoining`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#associatedconstant.LeftJoining) for more information.
         */
        LeftJoining = 3,
        /**
         * See the [Rust documentation for `RightJoining`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#associatedconstant.RightJoining) for more information.
         */
        RightJoining = 4,
        /**
         * See the [Rust documentation for `Transparent`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#associatedconstant.Transparent) for more information.
         */
        Transparent = 5,
    };

    JoiningType(): value(Value::NonJoining) {}

    // Implicit conversions between enum and ::Value
    constexpr JoiningType(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::JoiningType for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::JoiningType> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::JoiningType> try_from_str(std::string_view s);

    inline icu4x::capi::JoiningType AsFFI() const;
    inline static icu4x::JoiningType FromFFI(icu4x::capi::JoiningType c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_JoiningType_D_HPP
