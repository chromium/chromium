#ifndef icu4x_GraphemeClusterBreakIteratorUtf16_D_HPP
#define icu4x_GraphemeClusterBreakIteratorUtf16_D_HPP

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
    struct GraphemeClusterBreakIteratorUtf16;
} // namespace capi
} // namespace

namespace icu4x {
class GraphemeClusterBreakIteratorUtf16 {
public:

  inline int32_t next();

  inline const icu4x::capi::GraphemeClusterBreakIteratorUtf16* AsFFI() const;
  inline icu4x::capi::GraphemeClusterBreakIteratorUtf16* AsFFI();
  inline static const icu4x::GraphemeClusterBreakIteratorUtf16* FromFFI(const icu4x::capi::GraphemeClusterBreakIteratorUtf16* ptr);
  inline static icu4x::GraphemeClusterBreakIteratorUtf16* FromFFI(icu4x::capi::GraphemeClusterBreakIteratorUtf16* ptr);
  inline static void operator delete(void* ptr);
private:
  GraphemeClusterBreakIteratorUtf16() = delete;
  GraphemeClusterBreakIteratorUtf16(const icu4x::GraphemeClusterBreakIteratorUtf16&) = delete;
  GraphemeClusterBreakIteratorUtf16(icu4x::GraphemeClusterBreakIteratorUtf16&&) noexcept = delete;
  GraphemeClusterBreakIteratorUtf16 operator=(const icu4x::GraphemeClusterBreakIteratorUtf16&) = delete;
  GraphemeClusterBreakIteratorUtf16 operator=(icu4x::GraphemeClusterBreakIteratorUtf16&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GraphemeClusterBreakIteratorUtf16_D_HPP
