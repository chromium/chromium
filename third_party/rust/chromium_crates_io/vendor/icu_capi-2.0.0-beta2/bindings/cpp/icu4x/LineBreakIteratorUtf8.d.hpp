#ifndef icu4x_LineBreakIteratorUtf8_D_HPP
#define icu4x_LineBreakIteratorUtf8_D_HPP

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
    struct LineBreakIteratorUtf8;
} // namespace capi
} // namespace

namespace icu4x {
class LineBreakIteratorUtf8 {
public:

  inline int32_t next();

  inline const icu4x::capi::LineBreakIteratorUtf8* AsFFI() const;
  inline icu4x::capi::LineBreakIteratorUtf8* AsFFI();
  inline static const icu4x::LineBreakIteratorUtf8* FromFFI(const icu4x::capi::LineBreakIteratorUtf8* ptr);
  inline static icu4x::LineBreakIteratorUtf8* FromFFI(icu4x::capi::LineBreakIteratorUtf8* ptr);
  inline static void operator delete(void* ptr);
private:
  LineBreakIteratorUtf8() = delete;
  LineBreakIteratorUtf8(const icu4x::LineBreakIteratorUtf8&) = delete;
  LineBreakIteratorUtf8(icu4x::LineBreakIteratorUtf8&&) noexcept = delete;
  LineBreakIteratorUtf8 operator=(const icu4x::LineBreakIteratorUtf8&) = delete;
  LineBreakIteratorUtf8 operator=(icu4x::LineBreakIteratorUtf8&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LineBreakIteratorUtf8_D_HPP
