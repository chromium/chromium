#ifndef icu4x_SentenceSegmenter_D_HPP
#define icu4x_SentenceSegmenter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

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
}


namespace icu4x {
namespace capi {
    struct SentenceSegmenter;
} // namespace capi
} // namespace

namespace icu4x {
class SentenceSegmenter {
public:

  inline static std::unique_ptr<icu4x::SentenceSegmenter> create();

  inline static diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError> create_with_content_locale(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError> create_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline std::unique_ptr<icu4x::SentenceBreakIteratorUtf8> segment(std::string_view input) const;

  inline std::unique_ptr<icu4x::SentenceBreakIteratorUtf16> segment16(std::u16string_view input) const;

  inline std::unique_ptr<icu4x::SentenceBreakIteratorLatin1> segment_latin1(diplomat::span<const uint8_t> input) const;

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
#endif // icu4x_SentenceSegmenter_D_HPP
