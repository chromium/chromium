#ifndef ICU4X_CodePointSetData_D_HPP
#define ICU4X_CodePointSetData_D_HPP

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
namespace capi { struct CodePointRangeIterator; }
class CodePointRangeIterator;
namespace capi { struct CodePointSetData; }
class CodePointSetData;
namespace capi { struct DataProvider; }
class DataProvider;
struct GeneralCategoryGroup;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CodePointSetData;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
 *
 * See the [Rust documentation for `properties`](https://docs.rs/icu/2.2.0/icu/properties/index.html) for more information.
 *
 * See the [Rust documentation for `CodePointSetData`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointSetData.html) for more information.
 *
 * See the [Rust documentation for `CodePointSetDataBorrowed`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointSetDataBorrowed.html) for more information.
 */
class CodePointSetData {
public:

  /**
   * Checks whether the code point is in the set.
   *
   * See the [Rust documentation for `contains`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointSetDataBorrowed.html#method.contains) for more information.
   */
  inline bool contains(char32_t cp) const;

  /**
   * Produces an iterator over ranges of code points contained in this set
   *
   * See the [Rust documentation for `iter_ranges`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointSetDataBorrowed.html#method.iter_ranges) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges() const;

  /**
   * Produces an iterator over ranges of code points not contained in this set
   *
   * See the [Rust documentation for `iter_ranges_complemented`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointSetDataBorrowed.html#method.iter_ranges_complemented) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_complemented() const;

  /**
   * Produces a set for obtaining General Category Group values
   * which is a mask with the same format as the `U_GC_XX_MASK` mask in ICU4C, using compiled data.
   *
   * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html) for more information.
   *
   * See the [Rust documentation for `get_set_for_value_group`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.get_set_for_value_group) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_general_category_group(icu4x::GeneralCategoryGroup group);

  /**
   * Produces a set for obtaining General Category Group values
   * which is a mask with the same format as the `U_GC_XX_MASK` mask in ICU4C, using a provided data source.
   *
   * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html) for more information.
   *
   * See the [Rust documentation for `get_set_for_value_group`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.get_set_for_value_group) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_general_category_group_with_provider(const icu4x::DataProvider& provider, uint32_t group);

  /**
   * Get the `Ascii_Hex_Digit` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool ascii_hex_digit_for_char(char32_t ch);

  /**
   * Create a set for the `Ascii_Hex_Digit` property, using compiled data.
   *
   * See the [Rust documentation for `AsciiHexDigit`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.AsciiHexDigit.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_ascii_hex_digit();

  /**
   * Create a set for the `Ascii_Hex_Digit` property, using a particular data source.
   *
   * See the [Rust documentation for `AsciiHexDigit`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.AsciiHexDigit.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ascii_hex_digit_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Alnum` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool alnum_for_char(char32_t ch);

  /**
   * Create a set for the `Alnum` property, using compiled data.
   *
   * See the [Rust documentation for `Alnum`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Alnum.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_alnum();

  /**
   * Create a set for the `Alnum` property, using a particular data source.
   *
   * See the [Rust documentation for `Alnum`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Alnum.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_alnum_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Alphabetic` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool alphabetic_for_char(char32_t ch);

  /**
   * Create a set for the `Alphabetic` property, using compiled data.
   *
   * See the [Rust documentation for `Alphabetic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Alphabetic.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_alphabetic();

  /**
   * Create a set for the `Alphabetic` property, using a particular data source.
   *
   * See the [Rust documentation for `Alphabetic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Alphabetic.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_alphabetic_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Bidi_Control` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool bidi_control_for_char(char32_t ch);

  /**
   * Create a set for the `Bidi_Control` property, using compiled data.
   *
   * See the [Rust documentation for `BidiControl`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiControl.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_bidi_control();

  /**
   * Create a set for the `Bidi_Control` property, using a particular data source.
   *
   * See the [Rust documentation for `BidiControl`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiControl.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_bidi_control_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Bidi_Mirrored` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool bidi_mirrored_for_char(char32_t ch);

  /**
   * Create a set for the `Bidi_Mirrored` property, using compiled data.
   *
   * See the [Rust documentation for `BidiMirrored`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiMirrored.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_bidi_mirrored();

  /**
   * Create a set for the `Bidi_Mirrored` property, using a particular data source.
   *
   * See the [Rust documentation for `BidiMirrored`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiMirrored.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_bidi_mirrored_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Blank` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool blank_for_char(char32_t ch);

  /**
   * Create a set for the `Blank` property, using compiled data.
   *
   * See the [Rust documentation for `Blank`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Blank.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_blank();

  /**
   * Create a set for the `Blank` property, using a particular data source.
   *
   * See the [Rust documentation for `Blank`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Blank.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_blank_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Cased` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool cased_for_char(char32_t ch);

  /**
   * Create a set for the `Cased` property, using compiled data.
   *
   * See the [Rust documentation for `Cased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Cased.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_cased();

  /**
   * Create a set for the `Cased` property, using a particular data source.
   *
   * See the [Rust documentation for `Cased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Cased.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_cased_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Case_Ignorable` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool case_ignorable_for_char(char32_t ch);

  /**
   * Create a set for the `Case_Ignorable` property, using compiled data.
   *
   * See the [Rust documentation for `CaseIgnorable`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CaseIgnorable.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_case_ignorable();

  /**
   * Create a set for the `Case_Ignorable` property, using a particular data source.
   *
   * See the [Rust documentation for `CaseIgnorable`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CaseIgnorable.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_case_ignorable_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Full_Composition_Exclusion` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool full_composition_exclusion_for_char(char32_t ch);

  /**
   * Create a set for the `Full_Composition_Exclusion` property, using compiled data.
   *
   * See the [Rust documentation for `FullCompositionExclusion`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.FullCompositionExclusion.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_full_composition_exclusion();

  /**
   * Create a set for the `Full_Composition_Exclusion` property, using a particular data source.
   *
   * See the [Rust documentation for `FullCompositionExclusion`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.FullCompositionExclusion.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_full_composition_exclusion_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Changes_When_Casefolded` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool changes_when_casefolded_for_char(char32_t ch);

  /**
   * Create a set for the `Changes_When_Casefolded` property, using compiled data.
   *
   * See the [Rust documentation for `ChangesWhenCasefolded`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenCasefolded.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_casefolded();

  /**
   * Create a set for the `Changes_When_Casefolded` property, using a particular data source.
   *
   * See the [Rust documentation for `ChangesWhenCasefolded`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenCasefolded.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_casefolded_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Changes_When_Casemapped` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool changes_when_casemapped_for_char(char32_t ch);

  /**
   * Create a set for the `Changes_When_Casemapped` property, using compiled data.
   *
   * See the [Rust documentation for `ChangesWhenCasemapped`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenCasemapped.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_casemapped();

  /**
   * Create a set for the `Changes_When_Casemapped` property, using a particular data source.
   *
   * See the [Rust documentation for `ChangesWhenCasemapped`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenCasemapped.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_casemapped_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Changes_When_Nfkc_Casefolded` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool changes_when_nfkc_casefolded_for_char(char32_t ch);

  /**
   * Create a set for the `Changes_When_Nfkc_Casefolded` property, using compiled data.
   *
   * See the [Rust documentation for `ChangesWhenNfkcCasefolded`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenNfkcCasefolded.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_nfkc_casefolded();

  /**
   * Create a set for the `Changes_When_Nfkc_Casefolded` property, using a particular data source.
   *
   * See the [Rust documentation for `ChangesWhenNfkcCasefolded`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenNfkcCasefolded.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_nfkc_casefolded_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Changes_When_Lowercased` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool changes_when_lowercased_for_char(char32_t ch);

  /**
   * Create a set for the `Changes_When_Lowercased` property, using compiled data.
   *
   * See the [Rust documentation for `ChangesWhenLowercased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenLowercased.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_lowercased();

  /**
   * Create a set for the `Changes_When_Lowercased` property, using a particular data source.
   *
   * See the [Rust documentation for `ChangesWhenLowercased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenLowercased.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_lowercased_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Changes_When_Titlecased` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool changes_when_titlecased_for_char(char32_t ch);

  /**
   * Create a set for the `Changes_When_Titlecased` property, using compiled data.
   *
   * See the [Rust documentation for `ChangesWhenTitlecased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenTitlecased.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_titlecased();

  /**
   * Create a set for the `Changes_When_Titlecased` property, using a particular data source.
   *
   * See the [Rust documentation for `ChangesWhenTitlecased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenTitlecased.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_titlecased_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Changes_When_Uppercased` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool changes_when_uppercased_for_char(char32_t ch);

  /**
   * Create a set for the `Changes_When_Uppercased` property, using compiled data.
   *
   * See the [Rust documentation for `ChangesWhenUppercased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenUppercased.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_uppercased();

  /**
   * Create a set for the `Changes_When_Uppercased` property, using a particular data source.
   *
   * See the [Rust documentation for `ChangesWhenUppercased`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ChangesWhenUppercased.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_uppercased_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Dash` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool dash_for_char(char32_t ch);

  /**
   * Create a set for the `Dash` property, using compiled data.
   *
   * See the [Rust documentation for `Dash`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Dash.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_dash();

  /**
   * Create a set for the `Dash` property, using a particular data source.
   *
   * See the [Rust documentation for `Dash`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Dash.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_dash_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Deprecated` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool deprecated_for_char(char32_t ch);

  /**
   * Create a set for the `Deprecated` property, using compiled data.
   *
   * See the [Rust documentation for `Deprecated`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Deprecated.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_deprecated();

  /**
   * Create a set for the `Deprecated` property, using a particular data source.
   *
   * See the [Rust documentation for `Deprecated`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Deprecated.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_deprecated_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Default_Ignorable_Code_Point` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool default_ignorable_code_point_for_char(char32_t ch);

  /**
   * Create a set for the `Default_Ignorable_Code_Point` property, using compiled data.
   *
   * See the [Rust documentation for `DefaultIgnorableCodePoint`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.DefaultIgnorableCodePoint.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_default_ignorable_code_point();

  /**
   * Create a set for the `Default_Ignorable_Code_Point` property, using a particular data source.
   *
   * See the [Rust documentation for `DefaultIgnorableCodePoint`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.DefaultIgnorableCodePoint.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_default_ignorable_code_point_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Diacritic` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool diacritic_for_char(char32_t ch);

  /**
   * Create a set for the `Diacritic` property, using compiled data.
   *
   * See the [Rust documentation for `Diacritic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Diacritic.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_diacritic();

  /**
   * Create a set for the `Diacritic` property, using a particular data source.
   *
   * See the [Rust documentation for `Diacritic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Diacritic.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_diacritic_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Emoji_Modifier_Base` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool emoji_modifier_base_for_char(char32_t ch);

  /**
   * Create a set for the `Emoji_Modifier_Base` property, using compiled data.
   *
   * See the [Rust documentation for `EmojiModifierBase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiModifierBase.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_modifier_base();

  /**
   * Create a set for the `Emoji_Modifier_Base` property, using a particular data source.
   *
   * See the [Rust documentation for `EmojiModifierBase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiModifierBase.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_modifier_base_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Emoji_Component` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool emoji_component_for_char(char32_t ch);

  /**
   * Create a set for the `Emoji_Component` property, using compiled data.
   *
   * See the [Rust documentation for `EmojiComponent`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiComponent.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_component();

  /**
   * Create a set for the `Emoji_Component` property, using a particular data source.
   *
   * See the [Rust documentation for `EmojiComponent`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiComponent.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_component_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Emoji_Modifier` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool emoji_modifier_for_char(char32_t ch);

  /**
   * Create a set for the `Emoji_Modifier` property, using compiled data.
   *
   * See the [Rust documentation for `EmojiModifier`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiModifier.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_modifier();

  /**
   * Create a set for the `Emoji_Modifier` property, using a particular data source.
   *
   * See the [Rust documentation for `EmojiModifier`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiModifier.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_modifier_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Emoji` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool emoji_for_char(char32_t ch);

  /**
   * Create a set for the `Emoji` property, using compiled data.
   *
   * See the [Rust documentation for `Emoji`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Emoji.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji();

  /**
   * Create a set for the `Emoji` property, using a particular data source.
   *
   * See the [Rust documentation for `Emoji`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Emoji.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Emoji_Presentation` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool emoji_presentation_for_char(char32_t ch);

  /**
   * Create a set for the `Emoji_Presentation` property, using compiled data.
   *
   * See the [Rust documentation for `EmojiPresentation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiPresentation.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_presentation();

  /**
   * Create a set for the `Emoji_Presentation` property, using a particular data source.
   *
   * See the [Rust documentation for `EmojiPresentation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EmojiPresentation.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_presentation_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Extender` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool extender_for_char(char32_t ch);

  /**
   * Create a set for the `Extender` property, using compiled data.
   *
   * See the [Rust documentation for `Extender`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Extender.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_extender();

  /**
   * Create a set for the `Extender` property, using a particular data source.
   *
   * See the [Rust documentation for `Extender`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Extender.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_extender_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Extended_Pictographic` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool extended_pictographic_for_char(char32_t ch);

  /**
   * Create a set for the `Extended_Pictographic` property, using compiled data.
   *
   * See the [Rust documentation for `ExtendedPictographic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ExtendedPictographic.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_extended_pictographic();

  /**
   * Create a set for the `Extended_Pictographic` property, using a particular data source.
   *
   * See the [Rust documentation for `ExtendedPictographic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ExtendedPictographic.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_extended_pictographic_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Graph` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool graph_for_char(char32_t ch);

  /**
   * Create a set for the `Graph` property, using compiled data.
   *
   * See the [Rust documentation for `Graph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Graph.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_graph();

  /**
   * Create a set for the `Graph` property, using a particular data source.
   *
   * See the [Rust documentation for `Graph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Graph.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_graph_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Grapheme_Base` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool grapheme_base_for_char(char32_t ch);

  /**
   * Create a set for the `Grapheme_Base` property, using compiled data.
   *
   * See the [Rust documentation for `GraphemeBase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeBase.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_grapheme_base();

  /**
   * Create a set for the `Grapheme_Base` property, using a particular data source.
   *
   * See the [Rust documentation for `GraphemeBase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeBase.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_grapheme_base_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Grapheme_Extend` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool grapheme_extend_for_char(char32_t ch);

  /**
   * Create a set for the `Grapheme_Extend` property, using compiled data.
   *
   * See the [Rust documentation for `GraphemeExtend`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeExtend.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_grapheme_extend();

  /**
   * Create a set for the `Grapheme_Extend` property, using a particular data source.
   *
   * See the [Rust documentation for `GraphemeExtend`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeExtend.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_grapheme_extend_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Grapheme_Link` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool grapheme_link_for_char(char32_t ch);

  /**
   * Create a set for the `Grapheme_Link` property, using compiled data.
   *
   * See the [Rust documentation for `GraphemeLink`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeLink.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_grapheme_link();

  /**
   * Create a set for the `Grapheme_Link` property, using a particular data source.
   *
   * See the [Rust documentation for `GraphemeLink`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeLink.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_grapheme_link_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Hex_Digit` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool hex_digit_for_char(char32_t ch);

  /**
   * Create a set for the `Hex_Digit` property, using compiled data.
   *
   * See the [Rust documentation for `HexDigit`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HexDigit.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_hex_digit();

  /**
   * Create a set for the `Hex_Digit` property, using a particular data source.
   *
   * See the [Rust documentation for `HexDigit`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HexDigit.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_hex_digit_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Hyphen` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool hyphen_for_char(char32_t ch);

  /**
   * Create a set for the `Hyphen` property, using compiled data.
   *
   * See the [Rust documentation for `Hyphen`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Hyphen.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_hyphen();

  /**
   * Create a set for the `Hyphen` property, using a particular data source.
   *
   * See the [Rust documentation for `Hyphen`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Hyphen.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_hyphen_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `ID_Compat_Math_Continue` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool id_compat_math_continue_for_char(char32_t ch);

  /**
   * Create a set for the `ID_Compat_Math_Continue` property, using compiled data.
   *
   * See the [Rust documentation for `IdCompatMathContinue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdCompatMathContinue.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_id_compat_math_continue();

  /**
   * Create a set for the `ID_Compat_Math_Continue` property, using a particular data source.
   *
   * See the [Rust documentation for `IdCompatMathContinue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdCompatMathContinue.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_id_compat_math_continue_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `ID_Compat_Math_Start` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool id_compat_math_start_for_char(char32_t ch);

  /**
   * Create a set for the `ID_Compat_Math_Start` property, using compiled data.
   *
   * See the [Rust documentation for `IdCompatMathStart`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdCompatMathStart.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_id_compat_math_start();

  /**
   * Create a set for the `ID_Compat_Math_Start` property, using a particular data source.
   *
   * See the [Rust documentation for `IdCompatMathStart`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdCompatMathStart.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_id_compat_math_start_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Id_Continue` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool id_continue_for_char(char32_t ch);

  /**
   * Create a set for the `Id_Continue` property, using compiled data.
   *
   * See the [Rust documentation for `IdContinue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdContinue.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_id_continue();

  /**
   * Create a set for the `Id_Continue` property, using a particular data source.
   *
   * See the [Rust documentation for `IdContinue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdContinue.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_id_continue_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Ideographic` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool ideographic_for_char(char32_t ch);

  /**
   * Create a set for the `Ideographic` property, using compiled data.
   *
   * See the [Rust documentation for `Ideographic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Ideographic.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_ideographic();

  /**
   * Create a set for the `Ideographic` property, using a particular data source.
   *
   * See the [Rust documentation for `Ideographic`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Ideographic.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ideographic_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Id_Start` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool id_start_for_char(char32_t ch);

  /**
   * Create a set for the `Id_Start` property, using compiled data.
   *
   * See the [Rust documentation for `IdStart`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdStart.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_id_start();

  /**
   * Create a set for the `Id_Start` property, using a particular data source.
   *
   * See the [Rust documentation for `IdStart`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdStart.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_id_start_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Ids_Binary_Operator` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool ids_binary_operator_for_char(char32_t ch);

  /**
   * Create a set for the `Ids_Binary_Operator` property, using compiled data.
   *
   * See the [Rust documentation for `IdsBinaryOperator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdsBinaryOperator.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_ids_binary_operator();

  /**
   * Create a set for the `Ids_Binary_Operator` property, using a particular data source.
   *
   * See the [Rust documentation for `IdsBinaryOperator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdsBinaryOperator.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ids_binary_operator_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Ids_Trinary_Operator` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool ids_trinary_operator_for_char(char32_t ch);

  /**
   * Create a set for the `Ids_Trinary_Operator` property, using compiled data.
   *
   * See the [Rust documentation for `IdsTrinaryOperator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdsTrinaryOperator.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_ids_trinary_operator();

  /**
   * Create a set for the `Ids_Trinary_Operator` property, using a particular data source.
   *
   * See the [Rust documentation for `IdsTrinaryOperator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdsTrinaryOperator.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ids_trinary_operator_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Ids_Unary_Operator` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool ids_unary_operator_for_char(char32_t ch);

  /**
   * Create a set for the `Ids_Unary_Operator` property, using compiled data.
   *
   * See the [Rust documentation for `IdsUnaryOperator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdsUnaryOperator.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_ids_unary_operator();

  /**
   * Create a set for the `Ids_Unary_Operator` property, using a particular data source.
   *
   * See the [Rust documentation for `IdsUnaryOperator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IdsUnaryOperator.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ids_unary_operator_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Join_Control` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool join_control_for_char(char32_t ch);

  /**
   * Create a set for the `Join_Control` property, using compiled data.
   *
   * See the [Rust documentation for `JoinControl`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoinControl.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_join_control();

  /**
   * Create a set for the `Join_Control` property, using a particular data source.
   *
   * See the [Rust documentation for `JoinControl`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoinControl.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_join_control_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Logical_Order_Exception` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool logical_order_exception_for_char(char32_t ch);

  /**
   * Create a set for the `Logical_Order_Exception` property, using compiled data.
   *
   * See the [Rust documentation for `LogicalOrderException`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LogicalOrderException.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_logical_order_exception();

  /**
   * Create a set for the `Logical_Order_Exception` property, using a particular data source.
   *
   * See the [Rust documentation for `LogicalOrderException`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LogicalOrderException.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_logical_order_exception_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Lowercase` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool lowercase_for_char(char32_t ch);

  /**
   * Create a set for the `Lowercase` property, using compiled data.
   *
   * See the [Rust documentation for `Lowercase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Lowercase.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_lowercase();

  /**
   * Create a set for the `Lowercase` property, using a particular data source.
   *
   * See the [Rust documentation for `Lowercase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Lowercase.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_lowercase_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Math` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool math_for_char(char32_t ch);

  /**
   * Create a set for the `Math` property, using compiled data.
   *
   * See the [Rust documentation for `Math`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Math.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_math();

  /**
   * Create a set for the `Math` property, using a particular data source.
   *
   * See the [Rust documentation for `Math`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Math.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_math_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Modifier_Combining_mark` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool modifier_combining_mark_for_char(char32_t ch);

  /**
   * Create a set for the `Modifier_Combining_mark` property, using compiled data.
   *
   * See the [Rust documentation for `ModifierCombiningMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ModifierCombiningMark.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_modifier_combining_mark();

  /**
   * Create a set for the `Modifier_Combining_mark` property, using a particular data source.
   *
   * See the [Rust documentation for `ModifierCombiningMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.ModifierCombiningMark.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_modifier_combining_mark_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Noncharacter_Code_Point` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool noncharacter_code_point_for_char(char32_t ch);

  /**
   * Create a set for the `Noncharacter_Code_Point` property, using compiled data.
   *
   * See the [Rust documentation for `NoncharacterCodePoint`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NoncharacterCodePoint.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_noncharacter_code_point();

  /**
   * Create a set for the `Noncharacter_Code_Point` property, using a particular data source.
   *
   * See the [Rust documentation for `NoncharacterCodePoint`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NoncharacterCodePoint.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_noncharacter_code_point_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Nfc_Inert` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool nfc_inert_for_char(char32_t ch);

  /**
   * Create a set for the `Nfc_Inert` property, using compiled data.
   *
   * See the [Rust documentation for `NfcInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfcInert.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfc_inert();

  /**
   * Create a set for the `Nfc_Inert` property, using a particular data source.
   *
   * See the [Rust documentation for `NfcInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfcInert.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfc_inert_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Nfd_Inert` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool nfd_inert_for_char(char32_t ch);

  /**
   * Create a set for the `Nfd_Inert` property, using compiled data.
   *
   * See the [Rust documentation for `NfdInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfdInert.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfd_inert();

  /**
   * Create a set for the `Nfd_Inert` property, using a particular data source.
   *
   * See the [Rust documentation for `NfdInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfdInert.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfd_inert_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Nfkc_Inert` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool nfkc_inert_for_char(char32_t ch);

  /**
   * Create a set for the `Nfkc_Inert` property, using compiled data.
   *
   * See the [Rust documentation for `NfkcInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfkcInert.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfkc_inert();

  /**
   * Create a set for the `Nfkc_Inert` property, using a particular data source.
   *
   * See the [Rust documentation for `NfkcInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfkcInert.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfkc_inert_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Nfkd_Inert` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool nfkd_inert_for_char(char32_t ch);

  /**
   * Create a set for the `Nfkd_Inert` property, using compiled data.
   *
   * See the [Rust documentation for `NfkdInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfkdInert.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfkd_inert();

  /**
   * Create a set for the `Nfkd_Inert` property, using a particular data source.
   *
   * See the [Rust documentation for `NfkdInert`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NfkdInert.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfkd_inert_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Pattern_Syntax` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool pattern_syntax_for_char(char32_t ch);

  /**
   * Create a set for the `Pattern_Syntax` property, using compiled data.
   *
   * See the [Rust documentation for `PatternSyntax`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.PatternSyntax.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_pattern_syntax();

  /**
   * Create a set for the `Pattern_Syntax` property, using a particular data source.
   *
   * See the [Rust documentation for `PatternSyntax`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.PatternSyntax.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_pattern_syntax_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Pattern_White_Space` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool pattern_white_space_for_char(char32_t ch);

  /**
   * Create a set for the `Pattern_White_Space` property, using compiled data.
   *
   * See the [Rust documentation for `PatternWhiteSpace`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.PatternWhiteSpace.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_pattern_white_space();

  /**
   * Create a set for the `Pattern_White_Space` property, using a particular data source.
   *
   * See the [Rust documentation for `PatternWhiteSpace`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.PatternWhiteSpace.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_pattern_white_space_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Prepended_Concatenation_Mark` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool prepended_concatenation_mark_for_char(char32_t ch);

  /**
   * Create a set for the `Prepended_Concatenation_Mark` property, using compiled data.
   *
   * See the [Rust documentation for `PrependedConcatenationMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.PrependedConcatenationMark.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_prepended_concatenation_mark();

  /**
   * Create a set for the `Prepended_Concatenation_Mark` property, using a particular data source.
   *
   * See the [Rust documentation for `PrependedConcatenationMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.PrependedConcatenationMark.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_prepended_concatenation_mark_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Print` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool print_for_char(char32_t ch);

  /**
   * Create a set for the `Print` property, using compiled data.
   *
   * See the [Rust documentation for `Print`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Print.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_print();

  /**
   * Create a set for the `Print` property, using a particular data source.
   *
   * See the [Rust documentation for `Print`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Print.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_print_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Quotation_Mark` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool quotation_mark_for_char(char32_t ch);

  /**
   * Create a set for the `Quotation_Mark` property, using compiled data.
   *
   * See the [Rust documentation for `QuotationMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.QuotationMark.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_quotation_mark();

  /**
   * Create a set for the `Quotation_Mark` property, using a particular data source.
   *
   * See the [Rust documentation for `QuotationMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.QuotationMark.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_quotation_mark_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Radical` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool radical_for_char(char32_t ch);

  /**
   * Create a set for the `Radical` property, using compiled data.
   *
   * See the [Rust documentation for `Radical`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Radical.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_radical();

  /**
   * Create a set for the `Radical` property, using a particular data source.
   *
   * See the [Rust documentation for `Radical`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Radical.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_radical_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Regional_Indicator` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool regional_indicator_for_char(char32_t ch);

  /**
   * Create a set for the `Regional_Indicator` property, using compiled data.
   *
   * See the [Rust documentation for `RegionalIndicator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.RegionalIndicator.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_regional_indicator();

  /**
   * Create a set for the `Regional_Indicator` property, using a particular data source.
   *
   * See the [Rust documentation for `RegionalIndicator`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.RegionalIndicator.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_regional_indicator_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Soft_Dotted` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool soft_dotted_for_char(char32_t ch);

  /**
   * Create a set for the `Soft_Dotted` property, using compiled data.
   *
   * See the [Rust documentation for `SoftDotted`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SoftDotted.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_soft_dotted();

  /**
   * Create a set for the `Soft_Dotted` property, using a particular data source.
   *
   * See the [Rust documentation for `SoftDotted`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SoftDotted.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_soft_dotted_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Segment_Starter` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool segment_starter_for_char(char32_t ch);

  /**
   * Create a set for the `Segment_Starter` property, using compiled data.
   *
   * See the [Rust documentation for `SegmentStarter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SegmentStarter.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_segment_starter();

  /**
   * Create a set for the `Segment_Starter` property, using a particular data source.
   *
   * See the [Rust documentation for `SegmentStarter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SegmentStarter.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_segment_starter_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Case_Sensitive` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool case_sensitive_for_char(char32_t ch);

  /**
   * Create a set for the `Case_Sensitive` property, using compiled data.
   *
   * See the [Rust documentation for `CaseSensitive`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CaseSensitive.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_case_sensitive();

  /**
   * Create a set for the `Case_Sensitive` property, using a particular data source.
   *
   * See the [Rust documentation for `CaseSensitive`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CaseSensitive.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_case_sensitive_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Sentence_Terminal` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool sentence_terminal_for_char(char32_t ch);

  /**
   * Create a set for the `Sentence_Terminal` property, using compiled data.
   *
   * See the [Rust documentation for `SentenceTerminal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceTerminal.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_sentence_terminal();

  /**
   * Create a set for the `Sentence_Terminal` property, using a particular data source.
   *
   * See the [Rust documentation for `SentenceTerminal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceTerminal.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_sentence_terminal_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Terminal_Punctuation` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool terminal_punctuation_for_char(char32_t ch);

  /**
   * Create a set for the `Terminal_Punctuation` property, using compiled data.
   *
   * See the [Rust documentation for `TerminalPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.TerminalPunctuation.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_terminal_punctuation();

  /**
   * Create a set for the `Terminal_Punctuation` property, using a particular data source.
   *
   * See the [Rust documentation for `TerminalPunctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.TerminalPunctuation.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_terminal_punctuation_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Unified_Ideograph` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool unified_ideograph_for_char(char32_t ch);

  /**
   * Create a set for the `Unified_Ideograph` property, using compiled data.
   *
   * See the [Rust documentation for `UnifiedIdeograph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.UnifiedIdeograph.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_unified_ideograph();

  /**
   * Create a set for the `Unified_Ideograph` property, using a particular data source.
   *
   * See the [Rust documentation for `UnifiedIdeograph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.UnifiedIdeograph.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_unified_ideograph_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Uppercase` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool uppercase_for_char(char32_t ch);

  /**
   * Create a set for the `Uppercase` property, using compiled data.
   *
   * See the [Rust documentation for `Uppercase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Uppercase.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_uppercase();

  /**
   * Create a set for the `Uppercase` property, using a particular data source.
   *
   * See the [Rust documentation for `Uppercase`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Uppercase.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_uppercase_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Variation_Selector` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool variation_selector_for_char(char32_t ch);

  /**
   * Create a set for the `Variation_Selector` property, using compiled data.
   *
   * See the [Rust documentation for `VariationSelector`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VariationSelector.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_variation_selector();

  /**
   * Create a set for the `Variation_Selector` property, using a particular data source.
   *
   * See the [Rust documentation for `VariationSelector`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VariationSelector.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_variation_selector_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `White_Space` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool white_space_for_char(char32_t ch);

  /**
   * Create a set for the `White_Space` property, using compiled data.
   *
   * See the [Rust documentation for `WhiteSpace`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WhiteSpace.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_white_space();

  /**
   * Create a set for the `White_Space` property, using a particular data source.
   *
   * See the [Rust documentation for `WhiteSpace`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WhiteSpace.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_white_space_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Xdigit` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool xdigit_for_char(char32_t ch);

  /**
   * Create a set for the `Xdigit` property, using compiled data.
   *
   * See the [Rust documentation for `Xdigit`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Xdigit.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_xdigit();

  /**
   * Create a set for the `Xdigit` property, using a particular data source.
   *
   * See the [Rust documentation for `Xdigit`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Xdigit.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_xdigit_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Xid_Continue` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool xid_continue_for_char(char32_t ch);

  /**
   * Create a set for the `Xid_Continue` property, using compiled data.
   *
   * See the [Rust documentation for `XidContinue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.XidContinue.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_xid_continue();

  /**
   * Create a set for the `Xid_Continue` property, using a particular data source.
   *
   * See the [Rust documentation for `XidContinue`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.XidContinue.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_xid_continue_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Xid_Start` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.BinaryProperty.html#tymethod.for_char) for more information.
   */
  inline static bool xid_start_for_char(char32_t ch);

  /**
   * Create a set for the `Xid_Start` property, using compiled data.
   *
   * See the [Rust documentation for `XidStart`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.XidStart.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetData> create_xid_start();

  /**
   * Create a set for the `Xid_Start` property, using a particular data source.
   *
   * See the [Rust documentation for `XidStart`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.XidStart.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_xid_start_with_provider(const icu4x::DataProvider& provider);

  /**
   * [ecma]: https://tc39.es/ecma262/#table-binary-unicode-properties
   *
   * See the [Rust documentation for `new_for_ecma262`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointSetData.html#method.new_for_ecma262) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_for_ecma262(std::string_view property_name);

  /**
   * [ecma]: https://tc39.es/ecma262/#table-binary-unicode-properties
   *
   * See the [Rust documentation for `new_for_ecma262`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointSetData.html#method.new_for_ecma262) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_for_ecma262_with_provider(const icu4x::DataProvider& provider, std::string_view property_name);

    inline const icu4x::capi::CodePointSetData* AsFFI() const;
    inline icu4x::capi::CodePointSetData* AsFFI();
    inline static const icu4x::CodePointSetData* FromFFI(const icu4x::capi::CodePointSetData* ptr);
    inline static icu4x::CodePointSetData* FromFFI(icu4x::capi::CodePointSetData* ptr);
    inline static void operator delete(void* ptr);
private:
    CodePointSetData() = delete;
    CodePointSetData(const icu4x::CodePointSetData&) = delete;
    CodePointSetData(icu4x::CodePointSetData&&) noexcept = delete;
    CodePointSetData operator=(const icu4x::CodePointSetData&) = delete;
    CodePointSetData operator=(icu4x::CodePointSetData&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CodePointSetData_D_HPP
