#ifndef ICU4X_SentenceBreakIteratorUtf16_D_HPP
#define ICU4X_SentenceBreakIteratorUtf16_D_HPP

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
    struct SentenceBreakIteratorUtf16;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `SentenceBreakIterator`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.SentenceBreakIterator.html) for more information.
 */
class SentenceBreakIteratorUtf16 {
public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   *
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.SentenceBreakIterator.html#method.next) for more information.
   */
  inline int32_t next();

    inline const icu4x::capi::SentenceBreakIteratorUtf16* AsFFI() const;
    inline icu4x::capi::SentenceBreakIteratorUtf16* AsFFI();
    inline static const icu4x::SentenceBreakIteratorUtf16* FromFFI(const icu4x::capi::SentenceBreakIteratorUtf16* ptr);
    inline static icu4x::SentenceBreakIteratorUtf16* FromFFI(icu4x::capi::SentenceBreakIteratorUtf16* ptr);
    inline static void operator delete(void* ptr);
private:
    SentenceBreakIteratorUtf16() = delete;
    SentenceBreakIteratorUtf16(const icu4x::SentenceBreakIteratorUtf16&) = delete;
    SentenceBreakIteratorUtf16(icu4x::SentenceBreakIteratorUtf16&&) noexcept = delete;
    SentenceBreakIteratorUtf16 operator=(const icu4x::SentenceBreakIteratorUtf16&) = delete;
    SentenceBreakIteratorUtf16 operator=(icu4x::SentenceBreakIteratorUtf16&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_SentenceBreakIteratorUtf16_D_HPP
