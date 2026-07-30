#ifndef ICU4X_SentenceSegmenter_D_HPP
#define ICU4X_SentenceSegmenter_D_HPP

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
namespace capi { struct SentenceBreakIteratorLatin1; }
class SentenceBreakIteratorLatin1;
namespace capi { struct SentenceBreakIteratorUtf16; }
class SentenceBreakIteratorUtf16;
namespace capi { struct SentenceBreakIteratorUtf8; }
class SentenceBreakIteratorUtf8;
namespace capi { struct SentenceSegmenter; }
class SentenceSegmenter;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct SentenceSegmenter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.
 *
 * See the [Rust documentation for `SentenceSegmenter`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.SentenceSegmenter.html) for more information.
 */
class SentenceSegmenter {
public:

  /**
   * Construct a {@link SentenceSegmenter} using compiled data. This does not assume any content locale.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.SentenceSegmenter.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::SentenceSegmenter> create();

  /**
   * Construct a {@link SentenceSegmenter} for content known to be of a given locale, using compiled data.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError> create_with_content_locale(const icu4x::Locale& locale);

  /**
   * Construct a {@link SentenceSegmenter}  for content known to be of a given locale, using a particular data source.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError> create_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.SentenceSegmenterBorrowed.html#method.segment_utf8) for more information.
   */
  inline std::unique_ptr<icu4x::SentenceBreakIteratorUtf8> segment(std::string_view input) const;

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.SentenceSegmenterBorrowed.html#method.segment_utf16) for more information.
   */
  inline std::unique_ptr<icu4x::SentenceBreakIteratorUtf16> segment16(std::u16string_view input) const;

  /**
   * Segments a Latin-1 string.
   *
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.SentenceSegmenterBorrowed.html#method.segment_latin1) for more information.
   */
  inline std::unique_ptr<icu4x::SentenceBreakIteratorLatin1> segment_latin1(icu4x::diplomat::span<const uint8_t> input) const;

    inline const icu4x::capi::SentenceSegmenter* AsFFI() const;
    inline icu4x::capi::SentenceSegmenter* AsFFI();
    inline static const icu4x::SentenceSegmenter* FromFFI(const icu4x::capi::SentenceSegmenter* ptr);
    inline static icu4x::SentenceSegmenter* FromFFI(icu4x::capi::SentenceSegmenter* ptr);
    inline static void operator delete(void* ptr);
private:
    SentenceSegmenter() = delete;
    SentenceSegmenter(const icu4x::SentenceSegmenter&) = delete;
    SentenceSegmenter(icu4x::SentenceSegmenter&&) noexcept = delete;
    SentenceSegmenter operator=(const icu4x::SentenceSegmenter&) = delete;
    SentenceSegmenter operator=(icu4x::SentenceSegmenter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_SentenceSegmenter_D_HPP
