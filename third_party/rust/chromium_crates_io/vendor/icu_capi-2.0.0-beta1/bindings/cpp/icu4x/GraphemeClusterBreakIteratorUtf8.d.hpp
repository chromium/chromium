#ifndef icu4x_GraphemeClusterBreakIteratorUtf8_D_HPP
#define icu4x_GraphemeClusterBreakIteratorUtf8_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct GraphemeClusterBreakIteratorUtf8;
} // namespace capi
} // namespace

namespace icu4x {
class GraphemeClusterBreakIteratorUtf8 {
public:

  inline int32_t next();

  inline const icu4x::capi::GraphemeClusterBreakIteratorUtf8* AsFFI() const;
  inline icu4x::capi::GraphemeClusterBreakIteratorUtf8* AsFFI();
  inline static const icu4x::GraphemeClusterBreakIteratorUtf8* FromFFI(const icu4x::capi::GraphemeClusterBreakIteratorUtf8* ptr);
  inline static icu4x::GraphemeClusterBreakIteratorUtf8* FromFFI(icu4x::capi::GraphemeClusterBreakIteratorUtf8* ptr);
  inline static void operator delete(void* ptr);
private:
  GraphemeClusterBreakIteratorUtf8() = delete;
  GraphemeClusterBreakIteratorUtf8(const icu4x::GraphemeClusterBreakIteratorUtf8&) = delete;
  GraphemeClusterBreakIteratorUtf8(icu4x::GraphemeClusterBreakIteratorUtf8&&) noexcept = delete;
  GraphemeClusterBreakIteratorUtf8 operator=(const icu4x::GraphemeClusterBreakIteratorUtf8&) = delete;
  GraphemeClusterBreakIteratorUtf8 operator=(icu4x::GraphemeClusterBreakIteratorUtf8&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GraphemeClusterBreakIteratorUtf8_D_HPP
