#ifndef ICU4X_LocaleFallbackConfig_HPP
#define ICU4X_LocaleFallbackConfig_HPP

#include "LocaleFallbackConfig.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "LocaleFallbackPriority.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::LocaleFallbackConfig icu4x::LocaleFallbackConfig::AsFFI() const {
    return icu4x::capi::LocaleFallbackConfig {
        /* .priority = */ priority.AsFFI(),
    };
}

inline icu4x::LocaleFallbackConfig icu4x::LocaleFallbackConfig::FromFFI(icu4x::capi::LocaleFallbackConfig c_struct) {
    return icu4x::LocaleFallbackConfig {
        /* .priority = */ icu4x::LocaleFallbackPriority::FromFFI(c_struct.priority),
    };
}


#endif // ICU4X_LocaleFallbackConfig_HPP
