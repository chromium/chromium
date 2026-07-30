#ifndef ICU4X_LocaleFallbackConfig_D_HPP
#define ICU4X_LocaleFallbackConfig_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "LocaleFallbackPriority.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
class LocaleFallbackPriority;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleFallbackConfig {
      icu4x::capi::LocaleFallbackPriority priority;
    };

    typedef struct LocaleFallbackConfig_option {union { LocaleFallbackConfig ok; }; bool is_ok; } LocaleFallbackConfig_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * Collection of configurations for the ICU4X fallback algorithm.
 *
 * See the [Rust documentation for `LocaleFallbackConfig`](https://docs.rs/icu/2.2.0/icu/locale/fallback/struct.LocaleFallbackConfig.html) for more information.
 */
struct LocaleFallbackConfig {
    icu4x::LocaleFallbackPriority priority;

    inline icu4x::capi::LocaleFallbackConfig AsFFI() const;
    inline static icu4x::LocaleFallbackConfig FromFFI(icu4x::capi::LocaleFallbackConfig c_struct);
};

} // namespace
#endif // ICU4X_LocaleFallbackConfig_D_HPP
