#ifndef icu4x_SentenceBreakIteratorUtf16_D_HPP
#define icu4x_SentenceBreakIteratorUtf16_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct SentenceBreakIteratorUtf16;
} // namespace capi
} // namespace

namespace icu4x {
class SentenceBreakIteratorUtf16 {
public:

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
#endif // icu4x_SentenceBreakIteratorUtf16_D_HPP
