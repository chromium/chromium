#ifndef ICU4X_BidiClass_D_HPP
#define ICU4X_BidiClass_D_HPP

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
class BidiClass;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum BidiClass {
      BidiClass_LeftToRight = 0,
      BidiClass_RightToLeft = 1,
      BidiClass_EuropeanNumber = 2,
      BidiClass_EuropeanSeparator = 3,
      BidiClass_EuropeanTerminator = 4,
      BidiClass_ArabicNumber = 5,
      BidiClass_CommonSeparator = 6,
      BidiClass_ParagraphSeparator = 7,
      BidiClass_SegmentSeparator = 8,
      BidiClass_WhiteSpace = 9,
      BidiClass_OtherNeutral = 10,
      BidiClass_LeftToRightEmbedding = 11,
      BidiClass_LeftToRightOverride = 12,
      BidiClass_ArabicLetter = 13,
      BidiClass_RightToLeftEmbedding = 14,
      BidiClass_RightToLeftOverride = 15,
      BidiClass_PopDirectionalFormat = 16,
      BidiClass_NonspacingMark = 17,
      BidiClass_BoundaryNeutral = 18,
      BidiClass_FirstStrongIsolate = 19,
      BidiClass_LeftToRightIsolate = 20,
      BidiClass_RightToLeftIsolate = 21,
      BidiClass_PopDirectionalIsolate = 22,
    };

    typedef struct BidiClass_option {union { BidiClass ok; }; bool is_ok; } BidiClass_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `BidiClass`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html) for more information.
 */
class BidiClass {
public:
    enum Value {
        /**
         * See the [Rust documentation for `LeftToRight`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.LeftToRight) for more information.
         */
        LeftToRight = 0,
        /**
         * See the [Rust documentation for `RightToLeft`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.RightToLeft) for more information.
         */
        RightToLeft = 1,
        /**
         * See the [Rust documentation for `EuropeanNumber`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.EuropeanNumber) for more information.
         */
        EuropeanNumber = 2,
        /**
         * See the [Rust documentation for `EuropeanSeparator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.EuropeanSeparator) for more information.
         */
        EuropeanSeparator = 3,
        /**
         * See the [Rust documentation for `EuropeanTerminator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.EuropeanTerminator) for more information.
         */
        EuropeanTerminator = 4,
        /**
         * See the [Rust documentation for `ArabicNumber`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.ArabicNumber) for more information.
         */
        ArabicNumber = 5,
        /**
         * See the [Rust documentation for `CommonSeparator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.CommonSeparator) for more information.
         */
        CommonSeparator = 6,
        /**
         * See the [Rust documentation for `ParagraphSeparator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.ParagraphSeparator) for more information.
         */
        ParagraphSeparator = 7,
        /**
         * See the [Rust documentation for `SegmentSeparator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.SegmentSeparator) for more information.
         */
        SegmentSeparator = 8,
        /**
         * See the [Rust documentation for `WhiteSpace`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.WhiteSpace) for more information.
         */
        WhiteSpace = 9,
        /**
         * See the [Rust documentation for `OtherNeutral`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.OtherNeutral) for more information.
         */
        OtherNeutral = 10,
        /**
         * See the [Rust documentation for `LeftToRightEmbedding`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.LeftToRightEmbedding) for more information.
         */
        LeftToRightEmbedding = 11,
        /**
         * See the [Rust documentation for `LeftToRightOverride`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.LeftToRightOverride) for more information.
         */
        LeftToRightOverride = 12,
        /**
         * See the [Rust documentation for `ArabicLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.ArabicLetter) for more information.
         */
        ArabicLetter = 13,
        /**
         * See the [Rust documentation for `RightToLeftEmbedding`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.RightToLeftEmbedding) for more information.
         */
        RightToLeftEmbedding = 14,
        /**
         * See the [Rust documentation for `RightToLeftOverride`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.RightToLeftOverride) for more information.
         */
        RightToLeftOverride = 15,
        /**
         * See the [Rust documentation for `PopDirectionalFormat`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.PopDirectionalFormat) for more information.
         */
        PopDirectionalFormat = 16,
        /**
         * See the [Rust documentation for `NonspacingMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.NonspacingMark) for more information.
         */
        NonspacingMark = 17,
        /**
         * See the [Rust documentation for `BoundaryNeutral`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.BoundaryNeutral) for more information.
         */
        BoundaryNeutral = 18,
        /**
         * See the [Rust documentation for `FirstStrongIsolate`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.FirstStrongIsolate) for more information.
         */
        FirstStrongIsolate = 19,
        /**
         * See the [Rust documentation for `LeftToRightIsolate`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.LeftToRightIsolate) for more information.
         */
        LeftToRightIsolate = 20,
        /**
         * See the [Rust documentation for `RightToLeftIsolate`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.RightToLeftIsolate) for more information.
         */
        RightToLeftIsolate = 21,
        /**
         * See the [Rust documentation for `PopDirectionalIsolate`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#associatedconstant.PopDirectionalIsolate) for more information.
         */
        PopDirectionalIsolate = 22,
    };

    BidiClass(): value(Value::LeftToRight) {}

    // Implicit conversions between enum and ::Value
    constexpr BidiClass(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::BidiClass for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::BidiClass> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::BidiClass> try_from_str(std::string_view s);

    inline icu4x::capi::BidiClass AsFFI() const;
    inline static icu4x::BidiClass FromFFI(icu4x::capi::BidiClass c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_BidiClass_D_HPP
