#ifndef ICU4X_LocaleFallbackPriority_HPP
#define ICU4X_LocaleFallbackPriority_HPP

#include "LocaleFallbackPriority.d.hpp"

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

} // namespace capi
} // namespace

inline icu4x::capi::LocaleFallbackPriority icu4x::LocaleFallbackPriority::AsFFI() const {
    return static_cast<icu4x::capi::LocaleFallbackPriority>(value);
}

inline icu4x::LocaleFallbackPriority icu4x::LocaleFallbackPriority::FromFFI(icu4x::capi::LocaleFallbackPriority c_enum) {
    switch (c_enum) {
        case icu4x::capi::LocaleFallbackPriority_Language:
        case icu4x::capi::LocaleFallbackPriority_Region:
            return static_cast<icu4x::LocaleFallbackPriority::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_LocaleFallbackPriority_HPP
