#ifndef icu4x_WordBreakIteratorLatin1_D_HPP
#define icu4x_WordBreakIteratorLatin1_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class SegmenterWordType;
}


namespace icu4x {
namespace capi {
    struct WordBreakIteratorLatin1;
} // namespace capi
} // namespace

namespace icu4x {
class WordBreakIteratorLatin1 {
public:

  inline int32_t next();

  inline icu4x::SegmenterWordType word_type() const;

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
#endif // icu4x_WordBreakIteratorLatin1_D_HPP
