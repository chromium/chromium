#ifndef ICU4X_LocaleFallbackIterator_HPP
#define ICU4X_LocaleFallbackIterator_HPP

#include "LocaleFallbackIterator.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Locale.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::Locale* icu4x_LocaleFallbackIterator_next_mv1(icu4x::capi::LocaleFallbackIterator* self);

    void icu4x_LocaleFallbackIterator_destroy_mv1(LocaleFallbackIterator* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::Locale> icu4x::LocaleFallbackIterator::next() {
    auto result = icu4x::capi::icu4x_LocaleFallbackIterator_next_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::Locale>(icu4x::Locale::FromFFI(result));
}

inline const icu4x::capi::LocaleFallbackIterator* icu4x::LocaleFallbackIterator::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::LocaleFallbackIterator*>(this);
}

inline icu4x::capi::LocaleFallbackIterator* icu4x::LocaleFallbackIterator::AsFFI() {
    return reinterpret_cast<icu4x::capi::LocaleFallbackIterator*>(this);
}

inline const icu4x::LocaleFallbackIterator* icu4x::LocaleFallbackIterator::FromFFI(const icu4x::capi::LocaleFallbackIterator* ptr) {
    return reinterpret_cast<const icu4x::LocaleFallbackIterator*>(ptr);
}

inline icu4x::LocaleFallbackIterator* icu4x::LocaleFallbackIterator::FromFFI(icu4x::capi::LocaleFallbackIterator* ptr) {
    return reinterpret_cast<icu4x::LocaleFallbackIterator*>(ptr);
}

inline void icu4x::LocaleFallbackIterator::operator delete(void* ptr) {
    icu4x::capi::icu4x_LocaleFallbackIterator_destroy_mv1(reinterpret_cast<icu4x::capi::LocaleFallbackIterator*>(ptr));
}


#endif // ICU4X_LocaleFallbackIterator_HPP
