#ifndef ICU4X_LineSegmenter_D_HPP
#define ICU4X_LineSegmenter_D_HPP

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
namespace capi { struct LineBreakIteratorLatin1; }
class LineBreakIteratorLatin1;
namespace capi { struct LineBreakIteratorUtf16; }
class LineBreakIteratorUtf16;
namespace capi { struct LineBreakIteratorUtf8; }
class LineBreakIteratorUtf8;
namespace capi { struct LineSegmenter; }
class LineSegmenter;
namespace capi { struct Locale; }
class Locale;
struct LineBreakOptionsV2;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LineSegmenter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X line-break segmenter, capable of finding breakpoints in strings.
 *
 * See the [Rust documentation for `LineSegmenter`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html) for more information.
 */
class LineSegmenter {
public:

  /**
   * Construct a {@link LineSegmenter} with default options (no locale-based tailoring) using compiled data. It automatically loads the best
   * available payload data for Burmese, Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `new_auto`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_auto) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_auto();

  /**
   * Construct a {@link LineSegmenter} with default options (no locale-based tailoring) and LSTM payload data for
   * Burmese, Khmer, Lao, and Thai, using compiled data.
   *
   * See the [Rust documentation for `new_lstm`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_lstm) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_lstm();

  /**
   * Construct a {@link LineSegmenter} with default options (no locale-based tailoring) and dictionary payload data for
   * Burmese, Khmer, Lao, and Thai, using compiled data
   *
   * See the [Rust documentation for `new_dictionary`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_dictionary) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_dictionary();

  /**
   * Construct a {@link LineSegmenter} with default options (no locale-based tailoring) and no support for scripts requiring complex context dependent line breaks
   * (Burmese, Khmer, Lao, and Thai), using compiled data
   *
   * See the [Rust documentation for `new_for_non_complex_scripts`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_for_non_complex_scripts) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_for_non_complex_scripts();

  /**
   * Construct a {@link LineSegmenter} with custom options using compiled data. It automatically loads the best
   * available payload data for Burmese, Khmer, Lao, and Thai.
   *
   * See the [Rust documentation for `new_auto`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_auto) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_auto_with_options_v2(const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Construct a {@link LineSegmenter} with custom options. It automatically loads the best
   * available payload data for Burmese, Khmer, Lao, and Thai, using a particular data source.
   *
   * See the [Rust documentation for `new_auto`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_auto) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_auto_with_options_v2_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Construct a {@link LineSegmenter} with custom options and LSTM payload data for
   * Burmese, Khmer, Lao, and Thai, using compiled data.
   *
   * See the [Rust documentation for `new_lstm`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_lstm) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_lstm_with_options_v2(const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Construct a {@link LineSegmenter} with custom options and LSTM payload data for
   * Burmese, Khmer, Lao, and Thai, using a particular data source.
   *
   * See the [Rust documentation for `new_lstm`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_lstm) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_lstm_with_options_v2_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Construct a {@link LineSegmenter} with custom options and dictionary payload data for
   * Burmese, Khmer, Lao, and Thai, using compiled data.
   *
   * See the [Rust documentation for `new_dictionary`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_dictionary) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_dictionary_with_options_v2(const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Construct a {@link LineSegmenter} with custom options and dictionary payload data for
   * Burmese, Khmer, Lao, and Thai, using a particular data source.
   *
   * See the [Rust documentation for `new_dictionary`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_dictionary) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_dictionary_with_options_v2_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Construct a {@link LineSegmenter} with custom options and no support for scripts requiring complex context dependent line breaks
   * (Burmese, Khmer, Lao, and Thai), using compiled data.
   *
   * See the [Rust documentation for `new_for_non_complex_scripts`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_for_non_complex_scripts) for more information.
   */
  inline static std::unique_ptr<icu4x::LineSegmenter> create_for_non_complex_scripts_with_options_v2(const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Construct a {@link LineSegmenter} with custom options and no support for complex languages
   * (Burmese, Khmer, Lao, and Thai), using a particular data source.
   *
   * See the [Rust documentation for `new_for_non_complex_scripts`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenter.html#method.new_for_non_complex_scripts) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_for_non_complex_scripts_with_options_v2_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale* content_locale, icu4x::LineBreakOptionsV2 options);

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenterBorrowed.html#method.segment_utf8) for more information.
   */
  inline std::unique_ptr<icu4x::LineBreakIteratorUtf8> segment(std::string_view input) const;

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenterBorrowed.html#method.segment_utf16) for more information.
   */
  inline std::unique_ptr<icu4x::LineBreakIteratorUtf16> segment16(std::u16string_view input) const;

  /**
   * Segments a Latin-1 string.
   *
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.LineSegmenterBorrowed.html#method.segment_latin1) for more information.
   */
  inline std::unique_ptr<icu4x::LineBreakIteratorLatin1> segment_latin1(icu4x::diplomat::span<const uint8_t> input) const;

    inline const icu4x::capi::LineSegmenter* AsFFI() const;
    inline icu4x::capi::LineSegmenter* AsFFI();
    inline static const icu4x::LineSegmenter* FromFFI(const icu4x::capi::LineSegmenter* ptr);
    inline static icu4x::LineSegmenter* FromFFI(icu4x::capi::LineSegmenter* ptr);
    inline static void operator delete(void* ptr);
private:
    LineSegmenter() = delete;
    LineSegmenter(const icu4x::LineSegmenter&) = delete;
    LineSegmenter(icu4x::LineSegmenter&&) noexcept = delete;
    LineSegmenter operator=(const icu4x::LineSegmenter&) = delete;
    LineSegmenter operator=(icu4x::LineSegmenter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LineSegmenter_D_HPP
