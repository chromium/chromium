#ifndef ICU4X_GraphemeClusterBreak_D_HPP
#define ICU4X_GraphemeClusterBreak_D_HPP

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
class GraphemeClusterBreak;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum GraphemeClusterBreak {
      GraphemeClusterBreak_Other = 0,
      GraphemeClusterBreak_Control = 1,
      GraphemeClusterBreak_CR = 2,
      GraphemeClusterBreak_Extend = 3,
      GraphemeClusterBreak_L = 4,
      GraphemeClusterBreak_LF = 5,
      GraphemeClusterBreak_LV = 6,
      GraphemeClusterBreak_LVT = 7,
      GraphemeClusterBreak_T = 8,
      GraphemeClusterBreak_V = 9,
      GraphemeClusterBreak_SpacingMark = 10,
      GraphemeClusterBreak_Prepend = 11,
      GraphemeClusterBreak_RegionalIndicator = 12,
      GraphemeClusterBreak_EBase = 13,
      GraphemeClusterBreak_EBaseGAZ = 14,
      GraphemeClusterBreak_EModifier = 15,
      GraphemeClusterBreak_GlueAfterZwj = 16,
      GraphemeClusterBreak_ZWJ = 17,
    };

    typedef struct GraphemeClusterBreak_option {union { GraphemeClusterBreak ok; }; bool is_ok; } GraphemeClusterBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `GraphemeClusterBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html) for more information.
 */
class GraphemeClusterBreak {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Other`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.Other) for more information.
         */
        Other = 0,
        /**
         * See the [Rust documentation for `Control`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.Control) for more information.
         */
        Control = 1,
        /**
         * See the [Rust documentation for `CR`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.CR) for more information.
         */
        CR = 2,
        /**
         * See the [Rust documentation for `Extend`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.Extend) for more information.
         */
        Extend = 3,
        /**
         * See the [Rust documentation for `L`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.L) for more information.
         */
        L = 4,
        /**
         * See the [Rust documentation for `LF`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.LF) for more information.
         */
        LF = 5,
        /**
         * See the [Rust documentation for `LV`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.LV) for more information.
         */
        LV = 6,
        /**
         * See the [Rust documentation for `LVT`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.LVT) for more information.
         */
        LVT = 7,
        /**
         * See the [Rust documentation for `T`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.T) for more information.
         */
        T = 8,
        /**
         * See the [Rust documentation for `V`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.V) for more information.
         */
        V = 9,
        /**
         * See the [Rust documentation for `SpacingMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.SpacingMark) for more information.
         */
        SpacingMark = 10,
        /**
         * See the [Rust documentation for `Prepend`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.Prepend) for more information.
         */
        Prepend = 11,
        /**
         * See the [Rust documentation for `RegionalIndicator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.RegionalIndicator) for more information.
         */
        RegionalIndicator = 12,
        /**
         * See the [Rust documentation for `EBase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.EBase) for more information.
         */
        EBase = 13,
        /**
         * See the [Rust documentation for `EBaseGAZ`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.EBaseGAZ) for more information.
         */
        EBaseGAZ = 14,
        /**
         * See the [Rust documentation for `EModifier`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.EModifier) for more information.
         */
        EModifier = 15,
        /**
         * See the [Rust documentation for `GlueAfterZwj`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.GlueAfterZwj) for more information.
         */
        GlueAfterZwj = 16,
        /**
         * See the [Rust documentation for `ZWJ`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#associatedconstant.ZWJ) for more information.
         */
        ZWJ = 17,
    };

    GraphemeClusterBreak(): value(Value::Other) {}

    // Implicit conversions between enum and ::Value
    constexpr GraphemeClusterBreak(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::GraphemeClusterBreak for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::GraphemeClusterBreak> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::GraphemeClusterBreak> try_from_str(std::string_view s);

    inline icu4x::capi::GraphemeClusterBreak AsFFI() const;
    inline static icu4x::GraphemeClusterBreak FromFFI(icu4x::capi::GraphemeClusterBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_GraphemeClusterBreak_D_HPP
