#ifndef icu4x_SentenceBreakIteratorLatin1_D_HPP
#define icu4x_SentenceBreakIteratorLatin1_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct SentenceBreakIteratorLatin1;
} // namespace capi
} // namespace

namespace icu4x {
class SentenceBreakIteratorLatin1 {
public:

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
#endif // icu4x_SentenceBreakIteratorLatin1_D_HPP
