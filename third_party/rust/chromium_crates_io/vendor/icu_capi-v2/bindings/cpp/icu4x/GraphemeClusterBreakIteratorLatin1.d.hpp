#ifndef ICU4X_GraphemeClusterBreakIteratorLatin1_D_HPP
#define ICU4X_GraphemeClusterBreakIteratorLatin1_D_HPP

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
namespace capi {
    struct GraphemeClusterBreakIteratorLatin1;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `GraphemeClusterBreakIterator`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.GraphemeClusterBreakIterator.html) for more information.
 */
class GraphemeClusterBreakIteratorLatin1 {
public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   *
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.GraphemeClusterBreakIterator.html#method.next) for more information.
   */
  inline int32_t next();

    inline const icu4x::capi::GraphemeClusterBreakIteratorLatin1* AsFFI() const;
    inline icu4x::capi::GraphemeClusterBreakIteratorLatin1* AsFFI();
    inline static const icu4x::GraphemeClusterBreakIteratorLatin1* FromFFI(const icu4x::capi::GraphemeClusterBreakIteratorLatin1* ptr);
    inline static icu4x::GraphemeClusterBreakIteratorLatin1* FromFFI(icu4x::capi::GraphemeClusterBreakIteratorLatin1* ptr);
    inline static void operator delete(void* ptr);
private:
    GraphemeClusterBreakIteratorLatin1() = delete;
    GraphemeClusterBreakIteratorLatin1(const icu4x::GraphemeClusterBreakIteratorLatin1&) = delete;
    GraphemeClusterBreakIteratorLatin1(icu4x::GraphemeClusterBreakIteratorLatin1&&) noexcept = delete;
    GraphemeClusterBreakIteratorLatin1 operator=(const icu4x::GraphemeClusterBreakIteratorLatin1&) = delete;
    GraphemeClusterBreakIteratorLatin1 operator=(icu4x::GraphemeClusterBreakIteratorLatin1&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_GraphemeClusterBreakIteratorLatin1_D_HPP
