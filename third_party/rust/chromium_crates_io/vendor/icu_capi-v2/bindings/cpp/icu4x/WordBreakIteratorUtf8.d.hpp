#ifndef ICU4X_WordBreakIteratorUtf8_D_HPP
#define ICU4X_WordBreakIteratorUtf8_D_HPP

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
    struct WordBreakIteratorUtf8;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `WordBreakIterator`](https://docs.rs/icu/2.2.0/icu/segmenter/iterators/struct.WordBreakIterator.html) for more information.
 */
class WordBreakIteratorUtf8 {
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

    inline const icu4x::capi::WordBreakIteratorUtf8* AsFFI() const;
    inline icu4x::capi::WordBreakIteratorUtf8* AsFFI();
    inline static const icu4x::WordBreakIteratorUtf8* FromFFI(const icu4x::capi::WordBreakIteratorUtf8* ptr);
    inline static icu4x::WordBreakIteratorUtf8* FromFFI(icu4x::capi::WordBreakIteratorUtf8* ptr);
    inline static void operator delete(void* ptr);
private:
    WordBreakIteratorUtf8() = delete;
    WordBreakIteratorUtf8(const icu4x::WordBreakIteratorUtf8&) = delete;
    WordBreakIteratorUtf8(icu4x::WordBreakIteratorUtf8&&) noexcept = delete;
    WordBreakIteratorUtf8 operator=(const icu4x::WordBreakIteratorUtf8&) = delete;
    WordBreakIteratorUtf8 operator=(icu4x::WordBreakIteratorUtf8&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_WordBreakIteratorUtf8_D_HPP
