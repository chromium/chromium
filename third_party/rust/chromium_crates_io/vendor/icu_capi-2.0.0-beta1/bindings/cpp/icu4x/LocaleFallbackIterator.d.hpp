#ifndef icu4x_LocaleFallbackIterator_D_HPP
#define icu4x_LocaleFallbackIterator_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Locale; }
class Locale;
}


namespace icu4x {
namespace capi {
    struct LocaleFallbackIterator;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleFallbackIterator {
public:

  inline std::unique_ptr<icu4x::Locale> next();

  inline const icu4x::capi::LocaleFallbackIterator* AsFFI() const;
  inline icu4x::capi::LocaleFallbackIterator* AsFFI();
  inline static const icu4x::LocaleFallbackIterator* FromFFI(const icu4x::capi::LocaleFallbackIterator* ptr);
  inline static icu4x::LocaleFallbackIterator* FromFFI(icu4x::capi::LocaleFallbackIterator* ptr);
  inline static void operator delete(void* ptr);
private:
  LocaleFallbackIterator() = delete;
  LocaleFallbackIterator(const icu4x::LocaleFallbackIterator&) = delete;
  LocaleFallbackIterator(icu4x::LocaleFallbackIterator&&) noexcept = delete;
  LocaleFallbackIterator operator=(const icu4x::LocaleFallbackIterator&) = delete;
  LocaleFallbackIterator operator=(icu4x::LocaleFallbackIterator&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LocaleFallbackIterator_D_HPP
