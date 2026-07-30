#ifndef ICU4X_GeneralCategory_D_HPP
#define ICU4X_GeneralCategory_D_HPP

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
struct GeneralCategoryGroup;
class GeneralCategory;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum GeneralCategory {
      GeneralCategory_Unassigned = 0,
      GeneralCategory_UppercaseLetter = 1,
      GeneralCategory_LowercaseLetter = 2,
      GeneralCategory_TitlecaseLetter = 3,
      GeneralCategory_ModifierLetter = 4,
      GeneralCategory_OtherLetter = 5,
      GeneralCategory_NonspacingMark = 6,
      GeneralCategory_SpacingMark = 8,
      GeneralCategory_EnclosingMark = 7,
      GeneralCategory_DecimalNumber = 9,
      GeneralCategory_LetterNumber = 10,
      GeneralCategory_OtherNumber = 11,
      GeneralCategory_SpaceSeparator = 12,
      GeneralCategory_LineSeparator = 13,
      GeneralCategory_ParagraphSeparator = 14,
      GeneralCategory_Control = 15,
      GeneralCategory_Format = 16,
      GeneralCategory_PrivateUse = 17,
      GeneralCategory_Surrogate = 18,
      GeneralCategory_DashPunctuation = 19,
      GeneralCategory_OpenPunctuation = 20,
      GeneralCategory_ClosePunctuation = 21,
      GeneralCategory_ConnectorPunctuation = 22,
      GeneralCategory_InitialPunctuation = 28,
      GeneralCategory_FinalPunctuation = 29,
      GeneralCategory_OtherPunctuation = 23,
      GeneralCategory_MathSymbol = 24,
      GeneralCategory_CurrencySymbol = 25,
      GeneralCategory_ModifierSymbol = 26,
      GeneralCategory_OtherSymbol = 27,
    };

    typedef struct GeneralCategory_option {union { GeneralCategory ok; }; bool is_ok; } GeneralCategory_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `GeneralCategory`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html) for more information.
 */
class GeneralCategory {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Unassigned`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.Unassigned) for more information.
         */
        Unassigned = 0,
        /**
         * See the [Rust documentation for `UppercaseLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.UppercaseLetter) for more information.
         */
        UppercaseLetter = 1,
        /**
         * See the [Rust documentation for `LowercaseLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.LowercaseLetter) for more information.
         */
        LowercaseLetter = 2,
        /**
         * See the [Rust documentation for `TitlecaseLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.TitlecaseLetter) for more information.
         */
        TitlecaseLetter = 3,
        /**
         * See the [Rust documentation for `ModifierLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.ModifierLetter) for more information.
         */
        ModifierLetter = 4,
        /**
         * See the [Rust documentation for `OtherLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.OtherLetter) for more information.
         */
        OtherLetter = 5,
        /**
         * See the [Rust documentation for `NonspacingMark`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.NonspacingMark) for more information.
         */
        NonspacingMark = 6,
        /**
         * See the [Rust documentation for `SpacingMark`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.SpacingMark) for more information.
         */
        SpacingMark = 8,
        /**
         * See the [Rust documentation for `EnclosingMark`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.EnclosingMark) for more information.
         */
        EnclosingMark = 7,
        /**
         * See the [Rust documentation for `DecimalNumber`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.DecimalNumber) for more information.
         */
        DecimalNumber = 9,
        /**
         * See the [Rust documentation for `LetterNumber`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.LetterNumber) for more information.
         */
        LetterNumber = 10,
        /**
         * See the [Rust documentation for `OtherNumber`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.OtherNumber) for more information.
         */
        OtherNumber = 11,
        /**
         * See the [Rust documentation for `SpaceSeparator`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.SpaceSeparator) for more information.
         */
        SpaceSeparator = 12,
        /**
         * See the [Rust documentation for `LineSeparator`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.LineSeparator) for more information.
         */
        LineSeparator = 13,
        /**
         * See the [Rust documentation for `ParagraphSeparator`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.ParagraphSeparator) for more information.
         */
        ParagraphSeparator = 14,
        /**
         * See the [Rust documentation for `Control`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.Control) for more information.
         */
        Control = 15,
        /**
         * See the [Rust documentation for `Format`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.Format) for more information.
         */
        Format = 16,
        /**
         * See the [Rust documentation for `PrivateUse`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.PrivateUse) for more information.
         */
        PrivateUse = 17,
        /**
         * See the [Rust documentation for `Surrogate`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.Surrogate) for more information.
         */
        Surrogate = 18,
        /**
         * See the [Rust documentation for `DashPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.DashPunctuation) for more information.
         */
        DashPunctuation = 19,
        /**
         * See the [Rust documentation for `OpenPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.OpenPunctuation) for more information.
         */
        OpenPunctuation = 20,
        /**
         * See the [Rust documentation for `ClosePunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.ClosePunctuation) for more information.
         */
        ClosePunctuation = 21,
        /**
         * See the [Rust documentation for `ConnectorPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.ConnectorPunctuation) for more information.
         */
        ConnectorPunctuation = 22,
        /**
         * See the [Rust documentation for `InitialPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.InitialPunctuation) for more information.
         */
        InitialPunctuation = 28,
        /**
         * See the [Rust documentation for `FinalPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.FinalPunctuation) for more information.
         */
        FinalPunctuation = 29,
        /**
         * See the [Rust documentation for `OtherPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.OtherPunctuation) for more information.
         */
        OtherPunctuation = 23,
        /**
         * See the [Rust documentation for `MathSymbol`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.MathSymbol) for more information.
         */
        MathSymbol = 24,
        /**
         * See the [Rust documentation for `CurrencySymbol`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.CurrencySymbol) for more information.
         */
        CurrencySymbol = 25,
        /**
         * See the [Rust documentation for `ModifierSymbol`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.ModifierSymbol) for more information.
         */
        ModifierSymbol = 26,
        /**
         * See the [Rust documentation for `OtherSymbol`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html#variant.OtherSymbol) for more information.
         */
        OtherSymbol = 27,
    };

    GeneralCategory(): value(Value::Unassigned) {}

    // Implicit conversions between enum and ::Value
    constexpr GeneralCategory(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::GeneralCategory for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategory.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategory.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::GeneralCategory> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::GeneralCategory> try_from_str(std::string_view s);

  /**
   * Produces a `GeneralCategoryGroup` mask that can represent a group of general categories
   *
   * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html) for more information.
   */
  inline icu4x::GeneralCategoryGroup to_group() const;

    inline icu4x::capi::GeneralCategory AsFFI() const;
    inline static icu4x::GeneralCategory FromFFI(icu4x::capi::GeneralCategory c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_GeneralCategory_D_HPP
