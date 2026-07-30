#ifndef ICU4X_ExemplarCharacters_D_HPP
#define ICU4X_ExemplarCharacters_D_HPP

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
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct ExemplarCharacters; }
class ExemplarCharacters;
namespace capi { struct Locale; }
class Locale;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ExemplarCharacters;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A set of "exemplar characters" for a given locale.
 *
 * See the [Rust documentation for `locale`](https://docs.rs/icu/2.2.0/icu/locale/index.html) for more information.
 *
 * See the [Rust documentation for `ExemplarCharacters`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html) for more information.
 *
 * See the [Rust documentation for `ExemplarCharactersBorrowed`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharactersBorrowed.html) for more information.
 */
class ExemplarCharacters {
public:

  /**
   * Checks whether the string is in the set.
   *
   * See the [Rust documentation for `contains_str`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvliststringlist/struct.CodePointInversionListAndStringList.html#method.contains_str) for more information.
   */
  inline bool contains(std::string_view s) const;

  /**
   * Checks whether the code point is in the set.
   *
   * See the [Rust documentation for `contains`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvliststringlist/struct.CodePointInversionListAndStringList.html#method.contains) for more information.
   */
  inline bool contains(char32_t cp) const;

  /**
   * Create an {@link ExemplarCharacters} for the "main" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_main`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_main) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_main(const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "main" set of exemplar characters for a given locale, using a particular data source
   *
   * See the [Rust documentation for `try_new_main`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_main) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_main_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "auxiliary" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_auxiliary`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_auxiliary) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_auxiliary(const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "auxiliary" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_auxiliary`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_auxiliary) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_auxiliary_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "punctuation" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_punctuation`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_punctuation) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_punctuation(const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "punctuation" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_punctuation`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_punctuation) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_punctuation_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "numbers" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_numbers`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_numbers) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_numbers(const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "numbers" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_numbers`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_numbers) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_numbers_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "index" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_index`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_index) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_index(const icu4x::Locale& locale);

  /**
   * Create an {@link ExemplarCharacters} for the "index" set of exemplar characters for a given locale, using compiled data.
   *
   * See the [Rust documentation for `try_new_index`](https://docs.rs/icu/2.2.0/icu/locale/exemplar_chars/struct.ExemplarCharacters.html#method.try_new_index) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_index_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

    inline const icu4x::capi::ExemplarCharacters* AsFFI() const;
    inline icu4x::capi::ExemplarCharacters* AsFFI();
    inline static const icu4x::ExemplarCharacters* FromFFI(const icu4x::capi::ExemplarCharacters* ptr);
    inline static icu4x::ExemplarCharacters* FromFFI(icu4x::capi::ExemplarCharacters* ptr);
    inline static void operator delete(void* ptr);
private:
    ExemplarCharacters() = delete;
    ExemplarCharacters(const icu4x::ExemplarCharacters&) = delete;
    ExemplarCharacters(icu4x::ExemplarCharacters&&) noexcept = delete;
    ExemplarCharacters operator=(const icu4x::ExemplarCharacters&) = delete;
    ExemplarCharacters operator=(icu4x::ExemplarCharacters&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ExemplarCharacters_D_HPP
