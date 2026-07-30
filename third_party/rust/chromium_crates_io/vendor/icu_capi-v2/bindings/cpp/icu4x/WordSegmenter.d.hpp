#ifndef ICU4X_WordSegmenter_D_HPP
#define ICU4X_WordSegmenter_D_HPP

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
namespace capi { struct Locale; }
class Locale;
namespace capi { struct WordBreakIteratorLatin1; }
class WordBreakIteratorLatin1;
namespace capi { struct WordBreakIteratorUtf16; }
class WordBreakIteratorUtf16;
namespace capi { struct WordBreakIteratorUtf8; }
class WordBreakIteratorUtf8;
namespace capi { struct WordSegmenter; }
class WordSegmenter;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct WordSegmenter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X word-break segmenter, capable of finding word breakpoints in strings.
 *
 * See the [Rust documentation for `WordSegmenter`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html) for more information.
 */
class WordSegmenter {
public:

  /**
   * Construct a {@link WordSegmenter} with automatically selecting the best available LSTM
   * or dictionary payload data, using compiled data. This does not assume any content locale.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `new_auto`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.new_auto) for more information.
   */
  inline static std::unique_ptr<icu4x::WordSegmenter> create_auto();

  /**
   * Construct a {@link WordSegmenter} with automatically selecting the best available LSTM
   * or dictionary payload data, using compiled data.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `try_new_auto`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_auto) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_auto_with_content_locale(const icu4x::Locale& locale);

  /**
   * Construct a {@link WordSegmenter} with automatically selecting the best available LSTM
   * or dictionary payload data, using a particular data source.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `try_new_auto`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_auto) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_auto_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Construct a {@link WordSegmenter} with LSTM payload data for Burmese, Khmer, Lao, and
   * Thai, using compiled data. This does not assume any content locale.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `new_lstm`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.new_lstm) for more information.
   */
  inline static std::unique_ptr<icu4x::WordSegmenter> create_lstm();

  /**
   * Construct a {@link WordSegmenter} with LSTM payload data for Burmese, Khmer, Lao, and
   * Thai, using compiled data.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `try_new_lstm`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_lstm) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_lstm_with_content_locale(const icu4x::Locale& locale);

  /**
   * Construct a {@link WordSegmenter} with LSTM payload data for Burmese, Khmer, Lao, and
   * Thai, using a particular data source.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `try_new_lstm`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_lstm) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_lstm_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Construct a {@link WordSegmenter} with dictionary payload data for Chinese, Japanese,
   * Burmese, Khmer, Lao, and Thai, using compiled data. This does not assume any content locale.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and dictionary for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `new_dictionary`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.new_dictionary) for more information.
   */
  inline static std::unique_ptr<icu4x::WordSegmenter> create_dictionary();

  /**
   * Construct a {@link WordSegmenter} with dictionary payload data for Chinese, Japanese,
   * Burmese, Khmer, Lao, and Thai, using compiled data.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and dictionary for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `try_new_dictionary`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_dictionary) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_dictionary_with_content_locale(const icu4x::Locale& locale);

  /**
   * Construct a {@link WordSegmenter} with dictionary payload data for Chinese, Japanese,
   * Burmese, Khmer, Lao, and Thai, using a particular data source.
   *
   * Note: currently, it uses dictionary for Chinese and Japanese, and dictionary for Burmese,
   * Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `try_new_dictionary`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_dictionary) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_dictionary_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Construct a {@link WordSegmenter} with no support for scripts requiring complex context dependent word breaks (Chinese, Japanese,
   * Burmese, Khmer, Lao, and Thai), using compiled data. This does not assume any content locale.
   *
   * See the [Rust documentation for `new_for_non_complex_scripts`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.new_for_non_complex_scripts) for more information.
   */
  inline static std::unique_ptr<icu4x::WordSegmenter> create_for_non_complex_scripts();

  /**
   * Construct a {@link WordSegmenter} with no support for scripts requiring complex context dependent word breaks (Chinese, Japanese,
   * Burmese, Khmer, Lao, and Thai), using compiled data.
   *
   * See the [Rust documentation for `try_new_for_non_complex_scripts`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_for_non_complex_scripts) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_for_non_complex_scripts_with_content_locale(const icu4x::Locale& locale);

  /**
   * Construct a {@link WordSegmenter} with no support for scripts requiring complex context dependent word breaks (Chinese, Japanese,
   * Burmese, Khmer, Lao, and Thai), using a particular data source.
   *
   * See the [Rust documentation for `try_new_for_non_complex_scripts`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenter.html#method.try_new_for_non_complex_scripts) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> create_for_non_complex_scripts_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenterBorrowed.html#method.segment_utf8) for more information.
   */
  inline std::unique_ptr<icu4x::WordBreakIteratorUtf8> segment(std::string_view input) const;

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenterBorrowed.html#method.segment_utf16) for more information.
   */
  inline std::unique_ptr<icu4x::WordBreakIteratorUtf16> segment16(std::u16string_view input) const;

  /**
   * Segments a Latin-1 string.
   *
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.WordSegmenterBorrowed.html#method.segment_latin1) for more information.
   */
  inline std::unique_ptr<icu4x::WordBreakIteratorLatin1> segment_latin1(icu4x::diplomat::span<const uint8_t> input) const;

    inline const icu4x::capi::WordSegmenter* AsFFI() const;
    inline icu4x::capi::WordSegmenter* AsFFI();
    inline static const icu4x::WordSegmenter* FromFFI(const icu4x::capi::WordSegmenter* ptr);
    inline static icu4x::WordSegmenter* FromFFI(icu4x::capi::WordSegmenter* ptr);
    inline static void operator delete(void* ptr);
private:
    WordSegmenter() = delete;
    WordSegmenter(const icu4x::WordSegmenter&) = delete;
    WordSegmenter(icu4x::WordSegmenter&&) noexcept = delete;
    WordSegmenter operator=(const icu4x::WordSegmenter&) = delete;
    WordSegmenter operator=(icu4x::WordSegmenter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_WordSegmenter_D_HPP
