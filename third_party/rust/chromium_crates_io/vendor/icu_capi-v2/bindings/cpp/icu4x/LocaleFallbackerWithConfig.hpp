#ifndef ICU4X_LocaleFallbackerWithConfig_HPP
#define ICU4X_LocaleFallbackerWithConfig_HPP

#include "LocaleFallbackerWithConfig.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Locale.hpp"
#include "LocaleFallbackIterator.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::LocaleFallbackIterator* icu4x_LocaleFallbackerWithConfig_fallback_for_locale_mv1(const icu4x::capi::LocaleFallbackerWithConfig* self, const icu4x::capi::Locale* locale);

    void icu4x_LocaleFallbackerWithConfig_destroy_mv1(LocaleFallbackerWithConfig* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::LocaleFallbackIterator> icu4x::LocaleFallbackerWithConfig::fallback_for_locale(const icu4x::Locale& locale) const {
    auto result = icu4x::capi::icu4x_LocaleFallbackerWithConfig_fallback_for_locale_mv1(this->AsFFI(),
        locale.AsFFI());
    return std::unique_ptr<icu4x::LocaleFallbackIterator>(icu4x::LocaleFallbackIterator::FromFFI(result));
}

inline const icu4x::capi::LocaleFallbackerWithConfig* icu4x::LocaleFallbackerWithConfig::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::LocaleFallbackerWithConfig*>(this);
}

inline icu4x::capi::LocaleFallbackerWithConfig* icu4x::LocaleFallbackerWithConfig::AsFFI() {
    return reinterpret_cast<icu4x::capi::LocaleFallbackerWithConfig*>(this);
}

inline const icu4x::LocaleFallbackerWithConfig* icu4x::LocaleFallbackerWithConfig::FromFFI(const icu4x::capi::LocaleFallbackerWithConfig* ptr) {
    return reinterpret_cast<const icu4x::LocaleFallbackerWithConfig*>(ptr);
}

inline icu4x::LocaleFallbackerWithConfig* icu4x::LocaleFallbackerWithConfig::FromFFI(icu4x::capi::LocaleFallbackerWithConfig* ptr) {
    return reinterpret_cast<icu4x::LocaleFallbackerWithConfig*>(ptr);
}

inline void icu4x::LocaleFallbackerWithConfig::operator delete(void* ptr) {
    icu4x::capi::icu4x_LocaleFallbackerWithConfig_destroy_mv1(reinterpret_cast<icu4x::capi::LocaleFallbackerWithConfig*>(ptr));
}


#endif // ICU4X_LocaleFallbackerWithConfig_HPP
