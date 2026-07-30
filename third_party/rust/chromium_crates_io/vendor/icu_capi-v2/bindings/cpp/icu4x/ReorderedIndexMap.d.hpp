#ifndef ICU4X_ReorderedIndexMap_D_HPP
#define ICU4X_ReorderedIndexMap_D_HPP

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
namespace capi {
    struct ReorderedIndexMap;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Thin wrapper around a vector that maps visual indices to source indices
 *
 * `map[visualIndex] = sourceIndex`
 *
 * Produced by `reorder_visual()` on {@link Bidi}.
 */
class ReorderedIndexMap {
public:

  /**
   * Get this as a slice/array of indices
   */
  inline icu4x::diplomat::span<const size_t> as_slice() const;

  /**
   * The length of this map
   */
  inline size_t len() const;

  /**
   * Whether this map is empty
   */
  inline bool is_empty() const;

  /**
   * Get element at `index`. Returns 0 when out of bounds
   * (note that 0 is also a valid in-bounds value, please use `len()`
   * to avoid out-of-bounds)
   */
  inline size_t operator[](size_t index) const;

    inline const icu4x::capi::ReorderedIndexMap* AsFFI() const;
    inline icu4x::capi::ReorderedIndexMap* AsFFI();
    inline static const icu4x::ReorderedIndexMap* FromFFI(const icu4x::capi::ReorderedIndexMap* ptr);
    inline static icu4x::ReorderedIndexMap* FromFFI(icu4x::capi::ReorderedIndexMap* ptr);
    inline static void operator delete(void* ptr);
private:
    ReorderedIndexMap() = delete;
    ReorderedIndexMap(const icu4x::ReorderedIndexMap&) = delete;
    ReorderedIndexMap(icu4x::ReorderedIndexMap&&) noexcept = delete;
    ReorderedIndexMap operator=(const icu4x::ReorderedIndexMap&) = delete;
    ReorderedIndexMap operator=(icu4x::ReorderedIndexMap&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ReorderedIndexMap_D_HPP
