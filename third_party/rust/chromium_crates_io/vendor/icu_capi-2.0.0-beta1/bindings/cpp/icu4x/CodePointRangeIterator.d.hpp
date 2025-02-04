#ifndef icu4x_CodePointRangeIterator_D_HPP
#define icu4x_CodePointRangeIterator_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
struct CodePointRangeIteratorResult;
}


namespace icu4x {
namespace capi {
    struct CodePointRangeIterator;
} // namespace capi
} // namespace

namespace icu4x {
class CodePointRangeIterator {
public:

  inline icu4x::CodePointRangeIteratorResult next();

  inline const icu4x::capi::CodePointRangeIterator* AsFFI() const;
  inline icu4x::capi::CodePointRangeIterator* AsFFI();
  inline static const icu4x::CodePointRangeIterator* FromFFI(const icu4x::capi::CodePointRangeIterator* ptr);
  inline static icu4x::CodePointRangeIterator* FromFFI(icu4x::capi::CodePointRangeIterator* ptr);
  inline static void operator delete(void* ptr);
private:
  CodePointRangeIterator() = delete;
  CodePointRangeIterator(const icu4x::CodePointRangeIterator&) = delete;
  CodePointRangeIterator(icu4x::CodePointRangeIterator&&) noexcept = delete;
  CodePointRangeIterator operator=(const icu4x::CodePointRangeIterator&) = delete;
  CodePointRangeIterator operator=(icu4x::CodePointRangeIterator&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CodePointRangeIterator_D_HPP
