#ifndef icu4x_GraphemeClusterBreakIteratorLatin1_D_HPP
#define icu4x_GraphemeClusterBreakIteratorLatin1_D_HPP

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
    struct GraphemeClusterBreakIteratorLatin1;
} // namespace capi
} // namespace

namespace icu4x {
class GraphemeClusterBreakIteratorLatin1 {
public:

  inline int32_t next();

  inline const icu4x::capi::GraphemeClusterBreakIteratorLatin1* AsFFI() const;
  inline icu4x::capi::GraphemeClusterBreakIteratorLatin1* AsFFI();
  inline static const icu4x::GraphemeClusterBreakIteratorLatin1* FromFFI(const icu4x::capi::GraphemeClusterBreakIteratorLatin1* ptr);
  inline static icu4x::GraphemeClusterBreakIteratorLatin1* FromFFI(icu4x::capi::GraphemeClusterBreakIteratorLatin1* ptr);
  inline static void operator delete(void* ptr);
private:
  GraphemeClusterBreakIteratorLatin1() = delete;
  GraphemeClusterBreakIteratorLatin1(const icu4x::GraphemeClusterBreakIteratorLatin1&) = delete;
  GraphemeClusterBreakIteratorLatin1(icu4x::GraphemeClusterBreakIteratorLatin1&&) noexcept = delete;
  GraphemeClusterBreakIteratorLatin1 operator=(const icu4x::GraphemeClusterBreakIteratorLatin1&) = delete;
  GraphemeClusterBreakIteratorLatin1 operator=(icu4x::GraphemeClusterBreakIteratorLatin1&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GraphemeClusterBreakIteratorLatin1_D_HPP
