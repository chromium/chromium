#ifndef icu4x_WordBreakIteratorUtf16_D_HPP
#define icu4x_WordBreakIteratorUtf16_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class SegmenterWordType;
}


namespace icu4x {
namespace capi {
    struct WordBreakIteratorUtf16;
} // namespace capi
} // namespace

namespace icu4x {
class WordBreakIteratorUtf16 {
public:

  inline int32_t next();

  inline icu4x::SegmenterWordType word_type() const;

  inline bool is_word_like() const;

  inline const icu4x::capi::WordBreakIteratorUtf16* AsFFI() const;
  inline icu4x::capi::WordBreakIteratorUtf16* AsFFI();
  inline static const icu4x::WordBreakIteratorUtf16* FromFFI(const icu4x::capi::WordBreakIteratorUtf16* ptr);
  inline static icu4x::WordBreakIteratorUtf16* FromFFI(icu4x::capi::WordBreakIteratorUtf16* ptr);
  inline static void operator delete(void* ptr);
private:
  WordBreakIteratorUtf16() = delete;
  WordBreakIteratorUtf16(const icu4x::WordBreakIteratorUtf16&) = delete;
  WordBreakIteratorUtf16(icu4x::WordBreakIteratorUtf16&&) noexcept = delete;
  WordBreakIteratorUtf16 operator=(const icu4x::WordBreakIteratorUtf16&) = delete;
  WordBreakIteratorUtf16 operator=(icu4x::WordBreakIteratorUtf16&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_WordBreakIteratorUtf16_D_HPP
