#ifndef icu4x_LineBreakIteratorLatin1_D_HPP
#define icu4x_LineBreakIteratorLatin1_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct LineBreakIteratorLatin1;
} // namespace capi
} // namespace

namespace icu4x {
class LineBreakIteratorLatin1 {
public:

  inline int32_t next();

  inline const icu4x::capi::LineBreakIteratorLatin1* AsFFI() const;
  inline icu4x::capi::LineBreakIteratorLatin1* AsFFI();
  inline static const icu4x::LineBreakIteratorLatin1* FromFFI(const icu4x::capi::LineBreakIteratorLatin1* ptr);
  inline static icu4x::LineBreakIteratorLatin1* FromFFI(icu4x::capi::LineBreakIteratorLatin1* ptr);
  inline static void operator delete(void* ptr);
private:
  LineBreakIteratorLatin1() = delete;
  LineBreakIteratorLatin1(const icu4x::LineBreakIteratorLatin1&) = delete;
  LineBreakIteratorLatin1(icu4x::LineBreakIteratorLatin1&&) noexcept = delete;
  LineBreakIteratorLatin1 operator=(const icu4x::LineBreakIteratorLatin1&) = delete;
  LineBreakIteratorLatin1 operator=(icu4x::LineBreakIteratorLatin1&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LineBreakIteratorLatin1_D_HPP
