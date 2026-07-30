#ifndef ICU4X_CodePointRangeIterator_D_HPP
#define ICU4X_CodePointRangeIterator_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
struct CodePointRangeIteratorResult;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CodePointRangeIterator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An iterator over code point ranges, produced by `CodePointSetData` or
 * one of the `CodePointMapData` types
 */
class CodePointRangeIterator {
public:

  /**
   * Advance the iterator by one and return the next range.
   *
   * If the iterator is out of items, `done` will be true
   */
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
#endif // ICU4X_CodePointRangeIterator_D_HPP
