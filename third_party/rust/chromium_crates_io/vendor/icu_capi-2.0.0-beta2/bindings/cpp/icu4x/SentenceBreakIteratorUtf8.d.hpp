#ifndef icu4x_SentenceBreakIteratorUtf8_D_HPP
#define icu4x_SentenceBreakIteratorUtf8_D_HPP

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
    struct SentenceBreakIteratorUtf8;
} // namespace capi
} // namespace

namespace icu4x {
class SentenceBreakIteratorUtf8 {
public:

  inline int32_t next();

  inline const icu4x::capi::SentenceBreakIteratorUtf8* AsFFI() const;
  inline icu4x::capi::SentenceBreakIteratorUtf8* AsFFI();
  inline static const icu4x::SentenceBreakIteratorUtf8* FromFFI(const icu4x::capi::SentenceBreakIteratorUtf8* ptr);
  inline static icu4x::SentenceBreakIteratorUtf8* FromFFI(icu4x::capi::SentenceBreakIteratorUtf8* ptr);
  inline static void operator delete(void* ptr);
private:
  SentenceBreakIteratorUtf8() = delete;
  SentenceBreakIteratorUtf8(const icu4x::SentenceBreakIteratorUtf8&) = delete;
  SentenceBreakIteratorUtf8(icu4x::SentenceBreakIteratorUtf8&&) noexcept = delete;
  SentenceBreakIteratorUtf8 operator=(const icu4x::SentenceBreakIteratorUtf8&) = delete;
  SentenceBreakIteratorUtf8 operator=(icu4x::SentenceBreakIteratorUtf8&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_SentenceBreakIteratorUtf8_D_HPP
