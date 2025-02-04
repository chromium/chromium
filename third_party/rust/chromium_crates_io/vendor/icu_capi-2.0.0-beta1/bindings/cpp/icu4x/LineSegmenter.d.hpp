#ifndef icu4x_LineSegmenter_D_HPP
#define icu4x_LineSegmenter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

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
}


namespace icu4x {
namespace capi {
    struct LineSegmenter;
} // namespace capi
} // namespace

namespace icu4x {
class LineSegmenter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_auto(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_lstm(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_dictionary(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_auto_with_options_v2(const icu4x::DataProvider& provider, const icu4x::Locale& content_locale, icu4x::LineBreakOptionsV2 options);

  inline static diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_lstm_with_options_v2(const icu4x::DataProvider& provider, const icu4x::Locale& content_locale, icu4x::LineBreakOptionsV2 options);

  inline static diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> create_dictionary_with_options_v2(const icu4x::DataProvider& provider, const icu4x::Locale& content_locale, icu4x::LineBreakOptionsV2 options);

  inline std::unique_ptr<icu4x::LineBreakIteratorUtf8> segment(std::string_view input) const;

  inline std::unique_ptr<icu4x::LineBreakIteratorUtf16> segment16(std::u16string_view input) const;

  inline std::unique_ptr<icu4x::LineBreakIteratorLatin1> segment_latin1(diplomat::span<const uint8_t> input) const;

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
#endif // icu4x_LineSegmenter_D_HPP
