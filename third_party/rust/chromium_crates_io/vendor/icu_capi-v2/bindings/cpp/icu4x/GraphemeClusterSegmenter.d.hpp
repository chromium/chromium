#ifndef ICU4X_GraphemeClusterSegmenter_D_HPP
#define ICU4X_GraphemeClusterSegmenter_D_HPP

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
namespace capi { struct GraphemeClusterBreakIteratorLatin1; }
class GraphemeClusterBreakIteratorLatin1;
namespace capi { struct GraphemeClusterBreakIteratorUtf16; }
class GraphemeClusterBreakIteratorUtf16;
namespace capi { struct GraphemeClusterBreakIteratorUtf8; }
class GraphemeClusterBreakIteratorUtf8;
namespace capi { struct GraphemeClusterSegmenter; }
class GraphemeClusterSegmenter;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct GraphemeClusterSegmenter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X grapheme-cluster-break segmenter, capable of finding grapheme cluster breakpoints
 * in strings.
 *
 * See the [Rust documentation for `GraphemeClusterSegmenter`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.GraphemeClusterSegmenter.html) for more information.
 */
class GraphemeClusterSegmenter {
public:

  /**
   * Construct an {@link GraphemeClusterSegmenter} using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::GraphemeClusterSegmenter> create();

  /**
   * Construct an {@link GraphemeClusterSegmenter}.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::GraphemeClusterSegmenter>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.GraphemeClusterSegmenterBorrowed.html#method.segment_utf8) for more information.
   */
  inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf8> segment(std::string_view input) const;

  /**
   * Segments a string.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.GraphemeClusterSegmenterBorrowed.html#method.segment_utf16) for more information.
   */
  inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf16> segment16(std::u16string_view input) const;

  /**
   * Segments a Latin-1 string.
   *
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/2.2.0/icu/segmenter/struct.GraphemeClusterSegmenterBorrowed.html#method.segment_latin1) for more information.
   */
  inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorLatin1> segment_latin1(icu4x::diplomat::span<const uint8_t> input) const;

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
#endif // ICU4X_GraphemeClusterSegmenter_D_HPP
