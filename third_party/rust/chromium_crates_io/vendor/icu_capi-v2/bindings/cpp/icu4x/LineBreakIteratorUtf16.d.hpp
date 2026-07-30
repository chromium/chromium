#ifndef ICU4X_LineBreakIteratorUtf16_D_HPP
#define ICU4X_LineBreakIteratorUtf16_D_HPP

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
    struct LineBreakIteratorUtf16;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `LineBreakIterator`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.LineBreakIterator.html) for more information.
 */
class LineBreakIteratorUtf16 {
public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   *
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.LineBreakIterator.html#method.next) for more information.
   */
  inline int32_t next();

    inline const icu4x::capi::LineBreakIteratorUtf16* AsFFI() const;
    inline icu4x::capi::LineBreakIteratorUtf16* AsFFI();
    inline static const icu4x::LineBreakIteratorUtf16* FromFFI(const icu4x::capi::LineBreakIteratorUtf16* ptr);
    inline static icu4x::LineBreakIteratorUtf16* FromFFI(icu4x::capi::LineBreakIteratorUtf16* ptr);
    inline static void operator delete(void* ptr);
private:
    LineBreakIteratorUtf16() = delete;
    LineBreakIteratorUtf16(const icu4x::LineBreakIteratorUtf16&) = delete;
    LineBreakIteratorUtf16(icu4x::LineBreakIteratorUtf16&&) noexcept = delete;
    LineBreakIteratorUtf16 operator=(const icu4x::LineBreakIteratorUtf16&) = delete;
    LineBreakIteratorUtf16 operator=(icu4x::LineBreakIteratorUtf16&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LineBreakIteratorUtf16_D_HPP
