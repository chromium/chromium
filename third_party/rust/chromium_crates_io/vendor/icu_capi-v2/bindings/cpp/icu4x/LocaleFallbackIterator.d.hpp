#ifndef ICU4X_LocaleFallbackIterator_D_HPP
#define ICU4X_LocaleFallbackIterator_D_HPP

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
namespace capi { struct Locale; }
class Locale;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleFallbackIterator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An iterator over the locale under fallback.
 *
 * See the [Rust documentation for `LocaleFallbackIterator`](https://docs.rs/icu/2.2.0/icu/locale/fallback/struct.LocaleFallbackIterator.html) for more information.
 */
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
#endif // ICU4X_LocaleFallbackIterator_D_HPP
