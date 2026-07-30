#ifndef ICU4X_WordBreakIteratorLatin1_D_HPP
#define ICU4X_WordBreakIteratorLatin1_D_HPP

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
class SegmenterWordType;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct WordBreakIteratorLatin1;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `WordBreakIterator`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.WordBreakIterator.html) for more information.
 */
class WordBreakIteratorLatin1 {
public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   *
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.WordBreakIterator.html#method.next) for more information.
   */
  inline int32_t next();

  /**
   * Return the status value of break boundary.
   *
   * See the [Rust documentation for `word_type`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.WordBreakIterator.html#method.word_type) for more information.
   */
  inline icu4x::SegmenterWordType word_type() const;

  /**
   * Return true when break boundary is word-like such as letter/number/CJK
   *
   * See the [Rust documentation for `is_word_like`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.WordBreakIterator.html#method.is_word_like) for more information.
   */
  inline bool is_word_like() const;

    inline const icu4x::capi::WordBreakIteratorLatin1* AsFFI() const;
    inline icu4x::capi::WordBreakIteratorLatin1* AsFFI();
    inline static const icu4x::WordBreakIteratorLatin1* FromFFI(const icu4x::capi::WordBreakIteratorLatin1* ptr);
    inline static icu4x::WordBreakIteratorLatin1* FromFFI(icu4x::capi::WordBreakIteratorLatin1* ptr);
    inline static void operator delete(void* ptr);
private:
    WordBreakIteratorLatin1() = delete;
    WordBreakIteratorLatin1(const icu4x::WordBreakIteratorLatin1&) = delete;
    WordBreakIteratorLatin1(icu4x::WordBreakIteratorLatin1&&) noexcept = delete;
    WordBreakIteratorLatin1 operator=(const icu4x::WordBreakIteratorLatin1&) = delete;
    WordBreakIteratorLatin1 operator=(icu4x::WordBreakIteratorLatin1&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_WordBreakIteratorLatin1_D_HPP
