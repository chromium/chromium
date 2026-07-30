#ifndef ICU4X_SentenceBreakIteratorLatin1_D_HPP
#define ICU4X_SentenceBreakIteratorLatin1_D_HPP

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
    struct SentenceBreakIteratorLatin1;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `SentenceBreakIterator`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.SentenceBreakIterator.html) for more information.
 */
class SentenceBreakIteratorLatin1 {
public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   *
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.SentenceBreakIterator.html#method.next) for more information.
   */
  inline int32_t next();

    inline const icu4x::capi::SentenceBreakIteratorLatin1* AsFFI() const;
    inline icu4x::capi::SentenceBreakIteratorLatin1* AsFFI();
    inline static const icu4x::SentenceBreakIteratorLatin1* FromFFI(const icu4x::capi::SentenceBreakIteratorLatin1* ptr);
    inline static icu4x::SentenceBreakIteratorLatin1* FromFFI(icu4x::capi::SentenceBreakIteratorLatin1* ptr);
    inline static void operator delete(void* ptr);
private:
    SentenceBreakIteratorLatin1() = delete;
    SentenceBreakIteratorLatin1(const icu4x::SentenceBreakIteratorLatin1&) = delete;
    SentenceBreakIteratorLatin1(icu4x::SentenceBreakIteratorLatin1&&) noexcept = delete;
    SentenceBreakIteratorLatin1 operator=(const icu4x::SentenceBreakIteratorLatin1&) = delete;
    SentenceBreakIteratorLatin1 operator=(icu4x::SentenceBreakIteratorLatin1&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_SentenceBreakIteratorLatin1_D_HPP
