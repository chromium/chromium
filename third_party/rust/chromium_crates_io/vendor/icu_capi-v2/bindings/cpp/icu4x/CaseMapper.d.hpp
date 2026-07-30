#ifndef ICU4X_CaseMapper_D_HPP
#define ICU4X_CaseMapper_D_HPP

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
namespace capi { struct CaseMapper; }
class CaseMapper;
namespace capi { struct CodePointSetBuilder; }
class CodePointSetBuilder;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Locale; }
class Locale;
struct TitlecaseOptionsV1;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CaseMapper;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `CaseMapper`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapper.html) for more information.
 */
class CaseMapper {
public:

  /**
   * Construct a new `CaseMapper` instance using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapper.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::CaseMapper> create();

  /**
   * Construct a new `CaseMapper` instance using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapper.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Returns the full lowercase mapping of the given string
   *
   * See the [Rust documentation for `lowercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.lowercase) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> lowercase(std::string_view s, const icu4x::Locale& locale) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> lowercase_write(std::string_view s, const icu4x::Locale& locale, W& writeable_output) const;

  /**
   * Returns the full uppercase mapping of the given string
   *
   * See the [Rust documentation for `uppercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.uppercase) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> uppercase(std::string_view s, const icu4x::Locale& locale) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> uppercase_write(std::string_view s, const icu4x::Locale& locale, W& writeable_output) const;

  /**
   * Returns the full lowercase mapping of the given string, using compiled data (avoids having to allocate a `CaseMapper` object)
   *
   * See the [Rust documentation for `lowercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.lowercase) for more information.
   */
  inline static icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> lowercase_with_compiled_data(std::string_view s, const icu4x::Locale& locale);
  template<typename W>
  inline static icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> lowercase_with_compiled_data_write(std::string_view s, const icu4x::Locale& locale, W& writeable_output);

  /**
   * Returns the full uppercase mapping of the given string, using compiled data (avoids having to allocate a `CaseMapper` object)
   *
   * See the [Rust documentation for `uppercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.uppercase) for more information.
   */
  inline static icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> uppercase_with_compiled_data(std::string_view s, const icu4x::Locale& locale);
  template<typename W>
  inline static icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> uppercase_with_compiled_data_write(std::string_view s, const icu4x::Locale& locale, W& writeable_output);

  /**
   * Returns the full titlecase mapping of the given string, performing head adjustment without
   * loading additional data.
   * (if head adjustment is enabled in the options)
   *
   * The `v1` refers to the version of the options struct, which may change as we add more options
   *
   * See the [Rust documentation for `titlecase_segment_with_only_case_data`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.titlecase_segment_with_only_case_data) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> titlecase_segment_with_only_case_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> titlecase_segment_with_only_case_data_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable_output) const;

  /**
   * Returns the full titlecase mapping of the given string, performing head adjustment without
   * loading additional data, using compiled data (avoids having to allocate a `CaseMapper` object).
   *
   * (if head adjustment is enabled in the options)
   *
   * The `v1` refers to the version of the options struct, which may change as we add more options
   *
   * See the [Rust documentation for `titlecase_segment_with_only_case_data_to_string`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.titlecase_segment_with_only_case_data_to_string) for more information.
   */
  inline static icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> titlecase_segment_with_only_case_compiled_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options);
  template<typename W>
  inline static icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> titlecase_segment_with_only_case_compiled_data_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable_output);

  /**
   * Case-folds the characters in the given string
   *
   * See the [Rust documentation for `fold`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.fold) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> fold(std::string_view s) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> fold_write(std::string_view s, W& writeable_output) const;

  /**
   * Case-folds the characters in the given string
   * using Turkic (T) mappings for dotted/dotless I.
   *
   * See the [Rust documentation for `fold_turkic`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.fold_turkic) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> fold_turkic(std::string_view s) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> fold_turkic_write(std::string_view s, W& writeable_output) const;

  /**
   * Adds all simple case mappings and the full case folding for `c` to `builder`.
   * Also adds special case closure mappings.
   *
   * In other words, this adds all characters that this casemaps to, as
   * well as all characters that may casemap to this one.
   *
   * Note that since `CodePointSetBuilder` does not contain strings, this will
   * ignore string mappings.
   *
   * Identical to the similarly named method on `CaseMapCloser`, use that if you
   * plan on using string case closure mappings too.
   *
   * See the [Rust documentation for `add_case_closure_to`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.add_case_closure_to) for more information.
   */
  inline void add_case_closure_to(char32_t c, icu4x::CodePointSetBuilder& builder) const;

  /**
   * Returns the simple lowercase mapping of the given character.
   *
   * This function only implements simple and common mappings.
   * Full mappings, which can map one char to a string, are not included.
   * For full mappings, use `CaseMapperBorrowed::lowercase`.
   *
   * See the [Rust documentation for `simple_lowercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_lowercase) for more information.
   */
  inline char32_t simple_lowercase(char32_t ch) const;

  /**
   * Returns the simple lowercase mapping of the given character, using compiled data (avoids having to allocate a `CaseMapper` object)
   *
   * See the [Rust documentation for `simple_lowercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_lowercase) for more information.
   */
  inline static char32_t simple_lowercase_with_compiled_data(char32_t ch);

  /**
   * Returns the simple uppercase mapping of the given character.
   *
   * This function only implements simple and common mappings.
   * Full mappings, which can map one char to a string, are not included.
   * For full mappings, use `CaseMapperBorrowed::uppercase`.
   *
   * See the [Rust documentation for `simple_uppercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_uppercase) for more information.
   */
  inline char32_t simple_uppercase(char32_t ch) const;

  /**
   * Returns the simple uppercase mapping of the given character, using compiled data (avoids having to allocate a `CaseMapper` object)
   *
   * See the [Rust documentation for `simple_uppercase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_uppercase) for more information.
   */
  inline static char32_t simple_uppercase_with_compiled_data(char32_t ch);

  /**
   * Returns the simple titlecase mapping of the given character.
   *
   * This function only implements simple and common mappings.
   * Full mappings, which can map one char to a string, are not included.
   * For full mappings, use `CaseMapperBorrowed::titlecase_segment`.
   *
   * See the [Rust documentation for `simple_titlecase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_titlecase) for more information.
   */
  inline char32_t simple_titlecase(char32_t ch) const;

  /**
   * Returns the simple titlecase mapping of the given character, using compiled data (avoids having to allocate a `CaseMapper` object)
   *
   * See the [Rust documentation for `simple_titlecase`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_titlecase) for more information.
   */
  inline static char32_t simple_titlecase_with_compiled_data(char32_t ch);

  /**
   * Returns the simple casefolding of the given character.
   *
   * This function only implements simple folding.
   * For full folding, use `CaseMapperBorrowed::fold`.
   *
   * See the [Rust documentation for `simple_fold`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_fold) for more information.
   */
  inline char32_t simple_fold(char32_t ch) const;

  /**
   * Returns the simple casefolding of the given character, using compiled data (avoids having to allocate a `CaseMapper` object)
   *
   * See the [Rust documentation for `simple_fold`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_fold) for more information.
   */
  inline static char32_t simple_fold_with_compiled_data(char32_t ch);

  /**
   * Returns the simple casefolding of the given character in the Turkic locale.
   *
   * This function only implements simple folding.
   * For full folding, use `CaseMapperBorrowed::fold_turkic`.
   *
   * See the [Rust documentation for `simple_fold_turkic`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_fold_turkic) for more information.
   */
  inline char32_t simple_fold_turkic(char32_t ch) const;

  /**
   * Returns the simple Turkic casefolding of the given character, using compiled data (avoids having to allocate a `CaseMapper` object)
   *
   * See the [Rust documentation for `simple_fold_turkic`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapperBorrowed.html#method.simple_fold_turkic) for more information.
   */
  inline static char32_t simple_fold_turkic_with_compiled_data(char32_t ch);

    inline const icu4x::capi::CaseMapper* AsFFI() const;
    inline icu4x::capi::CaseMapper* AsFFI();
    inline static const icu4x::CaseMapper* FromFFI(const icu4x::capi::CaseMapper* ptr);
    inline static icu4x::CaseMapper* FromFFI(icu4x::capi::CaseMapper* ptr);
    inline static void operator delete(void* ptr);
private:
    CaseMapper() = delete;
    CaseMapper(const icu4x::CaseMapper&) = delete;
    CaseMapper(icu4x::CaseMapper&&) noexcept = delete;
    CaseMapper operator=(const icu4x::CaseMapper&) = delete;
    CaseMapper operator=(icu4x::CaseMapper&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CaseMapper_D_HPP
