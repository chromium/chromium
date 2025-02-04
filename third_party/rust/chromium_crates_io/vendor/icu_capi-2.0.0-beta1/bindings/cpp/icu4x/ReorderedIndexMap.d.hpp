#ifndef icu4x_ReorderedIndexMap_D_HPP
#define icu4x_ReorderedIndexMap_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct ReorderedIndexMap;
} // namespace capi
} // namespace

namespace icu4x {
class ReorderedIndexMap {
public:

  inline diplomat::span<const size_t> as_slice() const;

  inline size_t len() const;

  inline bool is_empty() const;

  inline size_t get(size_t index) const;

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
#endif // icu4x_ReorderedIndexMap_D_HPP
