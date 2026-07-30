#ifndef ICU4X_LocaleFallbackerWithConfig_D_HPP
#define ICU4X_LocaleFallbackerWithConfig_D_HPP

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
namespace capi { struct LocaleFallbackIterator; }
class LocaleFallbackIterator;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleFallbackerWithConfig;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An object that runs the ICU4X locale fallback algorithm with specific configurations.
 *
 * See the [Rust documentation for `LocaleFallbacker`](https://docs.rs/icu_locale/2.2.0/icu_locale/struct.LocaleFallbacker.html) for more information.
 *
 * See the [Rust documentation for `LocaleFallbackerWithConfig`](https://docs.rs/icu/2.2.0/icu/locale/fallback/struct.LocaleFallbackerWithConfig.html) for more information.
 */
class LocaleFallbackerWithConfig {
public:

  /**
   * Creates an iterator from a locale with each step of fallback.
   *
   * See the [Rust documentation for `fallback_for`](https://docs.rs/icu_locale/2.2.0/icu_locale/struct.LocaleFallbacker.html#method.fallback_for) for more information.
   */
  inline std::unique_ptr<icu4x::LocaleFallbackIterator> fallback_for_locale(const icu4x::Locale& locale) const;

    inline const icu4x::capi::LocaleFallbackerWithConfig* AsFFI() const;
    inline icu4x::capi::LocaleFallbackerWithConfig* AsFFI();
    inline static const icu4x::LocaleFallbackerWithConfig* FromFFI(const icu4x::capi::LocaleFallbackerWithConfig* ptr);
    inline static icu4x::LocaleFallbackerWithConfig* FromFFI(icu4x::capi::LocaleFallbackerWithConfig* ptr);
    inline static void operator delete(void* ptr);
private:
    LocaleFallbackerWithConfig() = delete;
    LocaleFallbackerWithConfig(const icu4x::LocaleFallbackerWithConfig&) = delete;
    LocaleFallbackerWithConfig(icu4x::LocaleFallbackerWithConfig&&) noexcept = delete;
    LocaleFallbackerWithConfig operator=(const icu4x::LocaleFallbackerWithConfig&) = delete;
    LocaleFallbackerWithConfig operator=(icu4x::LocaleFallbackerWithConfig&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LocaleFallbackerWithConfig_D_HPP
