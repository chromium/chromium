#ifndef icu4x_GraphemeClusterSegmenter_D_HPP
#define icu4x_GraphemeClusterSegmenter_D_HPP

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
namespace capi { struct GraphemeClusterBreakIteratorLatin1; }
class GraphemeClusterBreakIteratorLatin1;
namespace capi { struct GraphemeClusterBreakIteratorUtf16; }
class GraphemeClusterBreakIteratorUtf16;
namespace capi { struct GraphemeClusterBreakIteratorUtf8; }
class GraphemeClusterBreakIteratorUtf8;
namespace capi { struct GraphemeClusterSegmenter; }
class GraphemeClusterSegmenter;
class DataError;
}


namespace icu4x {
namespace capi {
    struct GraphemeClusterSegmenter;
} // namespace capi
} // namespace

namespace icu4x {
class GraphemeClusterSegmenter {
public:

  inline static std::unique_ptr<icu4x::GraphemeClusterSegmenter> create();

  inline static diplomat::result<std::unique_ptr<icu4x::GraphemeClusterSegmenter>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf8> segment(std::string_view input) const;

  inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf16> segment16(std::u16string_view input) const;

  inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorLatin1> segment_latin1(diplomat::span<const uint8_t> input) const;

  inline const icu4x::capi::GraphemeClusterSegmenter* AsFFI() const;
  inline icu4x::capi::GraphemeClusterSegmenter* AsFFI();
  inline static const icu4x::GraphemeClusterSegmenter* FromFFI(const icu4x::capi::GraphemeClusterSegmenter* ptr);
  inline static icu4x::GraphemeClusterSegmenter* FromFFI(icu4x::capi::GraphemeClusterSegmenter* ptr);
  inline static void operator delete(void* ptr);
private:
  GraphemeClusterSegmenter() = delete;
  GraphemeClusterSegmenter(const icu4x::GraphemeClusterSegmenter&) = delete;
  GraphemeClusterSegmenter(icu4x::GraphemeClusterSegmenter&&) noexcept = delete;
  GraphemeClusterSegmenter operator=(const icu4x::GraphemeClusterSegmenter&) = delete;
  GraphemeClusterSegmenter operator=(icu4x::GraphemeClusterSegmenter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GraphemeClusterSegmenter_D_HPP
