#ifndef ICU4X_CodePointMapData8_D_HPP
#define ICU4X_CodePointMapData8_D_HPP

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
namespace capi { struct CodePointMapData8; }
class CodePointMapData8;
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
    struct CodePointMapData8;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.
 *
 * For properties whose values fit into 8 bits.
 *
 * See the [Rust documentation for `properties`](https://docs.rs/icu/2.2.0/icu/properties/index.html) for more information.
 *
 * See the [Rust documentation for `CodePointMapData`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapData.html) for more information.
 *
 * See the [Rust documentation for `CodePointMapDataBorrowed`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html) for more information.
 */
class CodePointMapData8 {
public:

  /**
   * Gets the value for a code point.
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.get) for more information.
   */
  inline uint8_t operator[](char32_t cp) const;

  /**
   * Produces an iterator over ranges of code points that map to `value`
   *
   * See the [Rust documentation for `iter_ranges_for_value`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value(uint8_t value) const;

  /**
   * Produces an iterator over ranges of code points that do not map to `value`
   *
   * See the [Rust documentation for `iter_ranges_for_value_complemented`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value_complemented) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value_complemented(uint8_t value) const;

  /**
   * Given a mask value (the nth bit marks property value = n), produce an iterator over ranges of code points
   * whose property values are contained in the mask.
   *
   * The main mask property supported is that for `General_Category`, which can be obtained via `general_category_to_mask()` or
   * by using `GeneralCategoryNameToMaskMapper`
   *
   * Should only be used on maps for properties with values less than 32 (like `General_Category`),
   * other maps will have unpredictable results
   *
   * See the [Rust documentation for `iter_ranges_for_group`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_group) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_group(icu4x::GeneralCategoryGroup group) const;

  /**
   * Gets a {@link CodePointSetData} representing all entries in this map that map to the given value
   *
   * See the [Rust documentation for `get_set_for_value`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.get_set_for_value) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointSetData> get_set_for_value(uint8_t value) const;

  /**
   * Create a map for the `BidiClass` property, using compiled data.
   *
   * See the [Rust documentation for `BidiClass`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_bidi_class();

  /**
   * Create a map for the `BidiClass` property, using a particular data source.
   *
   * See the [Rust documentation for `BidiClass`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_bidi_class_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `NumericType` property, using compiled data.
   *
   * See the [Rust documentation for `NumericType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_numeric_type();

  /**
   * Create a map for the `NumericType` property, using a particular data source.
   *
   * See the [Rust documentation for `NumericType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.NumericType.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_numeric_type_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `HangulSyllableType` property, using compiled data.
   *
   * See the [Rust documentation for `HangulSyllableType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_hangul_syllable_type();

  /**
   * Create a map for the `HangulSyllableType` property, using a particular data source.
   *
   * See the [Rust documentation for `HangulSyllableType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.HangulSyllableType.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_hangul_syllable_type_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `EastAsianWidth` property, using compiled data.
   *
   * See the [Rust documentation for `EastAsianWidth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EastAsianWidth.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_east_asian_width();

  /**
   * Create a map for the `EastAsianWidth` property, using a particular data source.
   *
   * See the [Rust documentation for `EastAsianWidth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.EastAsianWidth.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_east_asian_width_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `LineBreak` property, using compiled data.
   *
   * See the [Rust documentation for `LineBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_line_break();

  /**
   * Create a map for the `LineBreak` property, using a particular data source.
   *
   * See the [Rust documentation for `LineBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.LineBreak.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_line_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `GraphemeClusterBreak` property, using compiled data.
   *
   * See the [Rust documentation for `GraphemeClusterBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_grapheme_cluster_break();

  /**
   * Create a map for the `GraphemeClusterBreak` property, using a particular data source.
   *
   * See the [Rust documentation for `GraphemeClusterBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GraphemeClusterBreak.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_grapheme_cluster_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `WordBreak` property, using compiled data.
   *
   * See the [Rust documentation for `WordBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_word_break();

  /**
   * Create a map for the `WordBreak` property, using a particular data source.
   *
   * See the [Rust documentation for `WordBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.WordBreak.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_word_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `SentenceBreak` property, using compiled data.
   *
   * See the [Rust documentation for `SentenceBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_sentence_break();

  /**
   * Create a map for the `SentenceBreak` property, using a particular data source.
   *
   * See the [Rust documentation for `SentenceBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.SentenceBreak.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_sentence_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `CanonicalCombiningClass` property, using compiled data.
   *
   * See the [Rust documentation for `CanonicalCombiningClass`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_canonical_combining_class();

  /**
   * Create a map for the `CanonicalCombiningClass` property, using a particular data source.
   *
   * See the [Rust documentation for `CanonicalCombiningClass`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_canonical_combining_class_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `IndicSyllabicCategory` property, using compiled data.
   *
   * See the [Rust documentation for `IndicSyllabicCategory`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_indic_syllabic_category();

  /**
   * Create a map for the `IndicSyllabicCategory` property, using a particular data source.
   *
   * See the [Rust documentation for `IndicSyllabicCategory`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_indic_syllabic_category_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `IndicConjunctBreak` property, using compiled data.
   *
   * See the [Rust documentation for `IndicConjunctBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicConjunctBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_indic_conjunct_break();

  /**
   * Create a map for the `IndicConjunctBreak` property, using a particular data source.
   *
   * See the [Rust documentation for `IndicConjunctBreak`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicConjunctBreak.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_indic_conjunct_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `JoiningGroup` property, using compiled data.
   *
   * See the [Rust documentation for `JoiningGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_joining_group();

  /**
   * Create a map for the `JoiningGroup` property, using a particular data source.
   *
   * See the [Rust documentation for `JoiningGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_joining_group_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `JoiningType` property, using compiled data.
   *
   * See the [Rust documentation for `JoiningType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_joining_type();

  /**
   * Create a map for the `JoiningType` property, using a particular data source.
   *
   * See the [Rust documentation for `JoiningType`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningType.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_joining_type_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `GeneralCategory` property, using compiled data.
   *
   * See the [Rust documentation for `GeneralCategory`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_general_category();

  /**
   * Create a map for the `GeneralCategory` property, using a particular data source.
   *
   * See the [Rust documentation for `GeneralCategory`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_general_category_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a map for the `VerticalOrientation` property, using compiled data.
   *
   * See the [Rust documentation for `VerticalOrientation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData8> create_vertical_orientation();

  /**
   * Create a map for the `VerticalOrientation` property, using a particular data source.
   *
   * See the [Rust documentation for `VerticalOrientation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.VerticalOrientation.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_vertical_orientation_with_provider(const icu4x::DataProvider& provider);

    inline const icu4x::capi::CodePointMapData8* AsFFI() const;
    inline icu4x::capi::CodePointMapData8* AsFFI();
    inline static const icu4x::CodePointMapData8* FromFFI(const icu4x::capi::CodePointMapData8* ptr);
    inline static icu4x::CodePointMapData8* FromFFI(icu4x::capi::CodePointMapData8* ptr);
    inline static void operator delete(void* ptr);
private:
    CodePointMapData8() = delete;
    CodePointMapData8(const icu4x::CodePointMapData8&) = delete;
    CodePointMapData8(icu4x::CodePointMapData8&&) noexcept = delete;
    CodePointMapData8 operator=(const icu4x::CodePointMapData8&) = delete;
    CodePointMapData8 operator=(icu4x::CodePointMapData8&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CodePointMapData8_D_HPP
