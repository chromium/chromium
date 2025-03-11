#ifndef icu4x_WordBreakIteratorUtf8_D_HPP
#define icu4x_WordBreakIteratorUtf8_D_HPP

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
    struct WordBreakIteratorUtf8;
} // namespace capi
} // namespace

namespace icu4x {
class WordBreakIteratorUtf8 {
public:

  inline int32_t next();

  inline icu4x::SegmenterWordType word_type() const;

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
#endif // icu4x_WordBreakIteratorUtf8_D_HPP
