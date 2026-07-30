#ifndef ICU4X_CanonicalCombiningClass_D_HPP
#define ICU4X_CanonicalCombiningClass_D_HPP

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
class CanonicalCombiningClass;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum CanonicalCombiningClass {
      CanonicalCombiningClass_NotReordered = 0,
      CanonicalCombiningClass_Overlay = 1,
      CanonicalCombiningClass_HanReading = 6,
      CanonicalCombiningClass_Nukta = 7,
      CanonicalCombiningClass_KanaVoicing = 8,
      CanonicalCombiningClass_Virama = 9,
      CanonicalCombiningClass_CCC10 = 10,
      CanonicalCombiningClass_CCC11 = 11,
      CanonicalCombiningClass_CCC12 = 12,
      CanonicalCombiningClass_CCC13 = 13,
      CanonicalCombiningClass_CCC14 = 14,
      CanonicalCombiningClass_CCC15 = 15,
      CanonicalCombiningClass_CCC16 = 16,
      CanonicalCombiningClass_CCC17 = 17,
      CanonicalCombiningClass_CCC18 = 18,
      CanonicalCombiningClass_CCC19 = 19,
      CanonicalCombiningClass_CCC20 = 20,
      CanonicalCombiningClass_CCC21 = 21,
      CanonicalCombiningClass_CCC22 = 22,
      CanonicalCombiningClass_CCC23 = 23,
      CanonicalCombiningClass_CCC24 = 24,
      CanonicalCombiningClass_CCC25 = 25,
      CanonicalCombiningClass_CCC26 = 26,
      CanonicalCombiningClass_CCC27 = 27,
      CanonicalCombiningClass_CCC28 = 28,
      CanonicalCombiningClass_CCC29 = 29,
      CanonicalCombiningClass_CCC30 = 30,
      CanonicalCombiningClass_CCC31 = 31,
      CanonicalCombiningClass_CCC32 = 32,
      CanonicalCombiningClass_CCC33 = 33,
      CanonicalCombiningClass_CCC34 = 34,
      CanonicalCombiningClass_CCC35 = 35,
      CanonicalCombiningClass_CCC36 = 36,
      CanonicalCombiningClass_CCC84 = 84,
      CanonicalCombiningClass_CCC91 = 91,
      CanonicalCombiningClass_CCC103 = 103,
      CanonicalCombiningClass_CCC107 = 107,
      CanonicalCombiningClass_CCC118 = 118,
      CanonicalCombiningClass_CCC122 = 122,
      CanonicalCombiningClass_CCC129 = 129,
      CanonicalCombiningClass_CCC130 = 130,
      CanonicalCombiningClass_CCC132 = 132,
      CanonicalCombiningClass_CCC133 = 133,
      CanonicalCombiningClass_AttachedBelowLeft = 200,
      CanonicalCombiningClass_AttachedBelow = 202,
      CanonicalCombiningClass_AttachedAbove = 214,
      CanonicalCombiningClass_AttachedAboveRight = 216,
      CanonicalCombiningClass_BelowLeft = 218,
      CanonicalCombiningClass_Below = 220,
      CanonicalCombiningClass_BelowRight = 222,
      CanonicalCombiningClass_Left = 224,
      CanonicalCombiningClass_Right = 226,
      CanonicalCombiningClass_AboveLeft = 228,
      CanonicalCombiningClass_Above = 230,
      CanonicalCombiningClass_AboveRight = 232,
      CanonicalCombiningClass_DoubleBelow = 233,
      CanonicalCombiningClass_DoubleAbove = 234,
      CanonicalCombiningClass_IotaSubscript = 240,
    };

    typedef struct CanonicalCombiningClass_option {union { CanonicalCombiningClass ok; }; bool is_ok; } CanonicalCombiningClass_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `CanonicalCombiningClass`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html) for more information.
 */
class CanonicalCombiningClass {
public:
    enum Value {
        /**
         * See the [Rust documentation for `NotReordered`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.NotReordered) for more information.
         */
        NotReordered = 0,
        /**
         * See the [Rust documentation for `Overlay`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.Overlay) for more information.
         */
        Overlay = 1,
        /**
         * See the [Rust documentation for `HanReading`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.HanReading) for more information.
         */
        HanReading = 6,
        /**
         * See the [Rust documentation for `Nukta`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.Nukta) for more information.
         */
        Nukta = 7,
        /**
         * See the [Rust documentation for `KanaVoicing`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.KanaVoicing) for more information.
         */
        KanaVoicing = 8,
        /**
         * See the [Rust documentation for `Virama`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.Virama) for more information.
         */
        Virama = 9,
        /**
         * See the [Rust documentation for `CCC10`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC10) for more information.
         */
        CCC10 = 10,
        /**
         * See the [Rust documentation for `CCC11`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC11) for more information.
         */
        CCC11 = 11,
        /**
         * See the [Rust documentation for `CCC12`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC12) for more information.
         */
        CCC12 = 12,
        /**
         * See the [Rust documentation for `CCC13`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC13) for more information.
         */
        CCC13 = 13,
        /**
         * See the [Rust documentation for `CCC14`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC14) for more information.
         */
        CCC14 = 14,
        /**
         * See the [Rust documentation for `CCC15`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC15) for more information.
         */
        CCC15 = 15,
        /**
         * See the [Rust documentation for `CCC16`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC16) for more information.
         */
        CCC16 = 16,
        /**
         * See the [Rust documentation for `CCC17`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC17) for more information.
         */
        CCC17 = 17,
        /**
         * See the [Rust documentation for `CCC18`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC18) for more information.
         */
        CCC18 = 18,
        /**
         * See the [Rust documentation for `CCC19`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC19) for more information.
         */
        CCC19 = 19,
        /**
         * See the [Rust documentation for `CCC20`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC20) for more information.
         */
        CCC20 = 20,
        /**
         * See the [Rust documentation for `CCC21`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC21) for more information.
         */
        CCC21 = 21,
        /**
         * See the [Rust documentation for `CCC22`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC22) for more information.
         */
        CCC22 = 22,
        /**
         * See the [Rust documentation for `CCC23`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC23) for more information.
         */
        CCC23 = 23,
        /**
         * See the [Rust documentation for `CCC24`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC24) for more information.
         */
        CCC24 = 24,
        /**
         * See the [Rust documentation for `CCC25`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC25) for more information.
         */
        CCC25 = 25,
        /**
         * See the [Rust documentation for `CCC26`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC26) for more information.
         */
        CCC26 = 26,
        /**
         * See the [Rust documentation for `CCC27`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC27) for more information.
         */
        CCC27 = 27,
        /**
         * See the [Rust documentation for `CCC28`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC28) for more information.
         */
        CCC28 = 28,
        /**
         * See the [Rust documentation for `CCC29`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC29) for more information.
         */
        CCC29 = 29,
        /**
         * See the [Rust documentation for `CCC30`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC30) for more information.
         */
        CCC30 = 30,
        /**
         * See the [Rust documentation for `CCC31`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC31) for more information.
         */
        CCC31 = 31,
        /**
         * See the [Rust documentation for `CCC32`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC32) for more information.
         */
        CCC32 = 32,
        /**
         * See the [Rust documentation for `CCC33`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC33) for more information.
         */
        CCC33 = 33,
        /**
         * See the [Rust documentation for `CCC34`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC34) for more information.
         */
        CCC34 = 34,
        /**
         * See the [Rust documentation for `CCC35`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC35) for more information.
         */
        CCC35 = 35,
        /**
         * See the [Rust documentation for `CCC36`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC36) for more information.
         */
        CCC36 = 36,
        /**
         * See the [Rust documentation for `CCC84`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC84) for more information.
         */
        CCC84 = 84,
        /**
         * See the [Rust documentation for `CCC91`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC91) for more information.
         */
        CCC91 = 91,
        /**
         * See the [Rust documentation for `CCC103`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC103) for more information.
         */
        CCC103 = 103,
        /**
         * See the [Rust documentation for `CCC107`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC107) for more information.
         */
        CCC107 = 107,
        /**
         * See the [Rust documentation for `CCC118`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC118) for more information.
         */
        CCC118 = 118,
        /**
         * See the [Rust documentation for `CCC122`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC122) for more information.
         */
        CCC122 = 122,
        /**
         * See the [Rust documentation for `CCC129`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC129) for more information.
         */
        CCC129 = 129,
        /**
         * See the [Rust documentation for `CCC130`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC130) for more information.
         */
        CCC130 = 130,
        /**
         * See the [Rust documentation for `CCC132`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC132) for more information.
         */
        CCC132 = 132,
        /**
         * See the [Rust documentation for `CCC133`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.CCC133) for more information.
         */
        CCC133 = 133,
        /**
         * See the [Rust documentation for `AttachedBelowLeft`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.AttachedBelowLeft) for more information.
         */
        AttachedBelowLeft = 200,
        /**
         * See the [Rust documentation for `AttachedBelow`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.AttachedBelow) for more information.
         */
        AttachedBelow = 202,
        /**
         * See the [Rust documentation for `AttachedAbove`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.AttachedAbove) for more information.
         */
        AttachedAbove = 214,
        /**
         * See the [Rust documentation for `AttachedAboveRight`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.AttachedAboveRight) for more information.
         */
        AttachedAboveRight = 216,
        /**
         * See the [Rust documentation for `BelowLeft`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.BelowLeft) for more information.
         */
        BelowLeft = 218,
        /**
         * See the [Rust documentation for `Below`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.Below) for more information.
         */
        Below = 220,
        /**
         * See the [Rust documentation for `BelowRight`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.BelowRight) for more information.
         */
        BelowRight = 222,
        /**
         * See the [Rust documentation for `Left`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.Left) for more information.
         */
        Left = 224,
        /**
         * See the [Rust documentation for `Right`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.Right) for more information.
         */
        Right = 226,
        /**
         * See the [Rust documentation for `AboveLeft`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.AboveLeft) for more information.
         */
        AboveLeft = 228,
        /**
         * See the [Rust documentation for `Above`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.Above) for more information.
         */
        Above = 230,
        /**
         * See the [Rust documentation for `AboveRight`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.AboveRight) for more information.
         */
        AboveRight = 232,
        /**
         * See the [Rust documentation for `DoubleBelow`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.DoubleBelow) for more information.
         */
        DoubleBelow = 233,
        /**
         * See the [Rust documentation for `DoubleAbove`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.DoubleAbove) for more information.
         */
        DoubleAbove = 234,
        /**
         * See the [Rust documentation for `IotaSubscript`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#associatedconstant.IotaSubscript) for more information.
         */
        IotaSubscript = 240,
    };

    CanonicalCombiningClass(): value(Value::NotReordered) {}

    // Implicit conversions between enum and ::Value
    constexpr CanonicalCombiningClass(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::CanonicalCombiningClass for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::CanonicalCombiningClass> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::CanonicalCombiningClass> try_from_str(std::string_view s);

    inline icu4x::capi::CanonicalCombiningClass AsFFI() const;
    inline static icu4x::CanonicalCombiningClass FromFFI(icu4x::capi::CanonicalCombiningClass c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CanonicalCombiningClass_D_HPP
