#ifndef ICU4X_CollatorAlternateHandling_HPP
#define ICU4X_CollatorAlternateHandling_HPP

#include "CollatorAlternateHandling.d.hpp"

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

inline icu4x::capi::CollatorAlternateHandling icu4x::CollatorAlternateHandling::AsFFI() const {
    return static_cast<icu4x::capi::CollatorAlternateHandling>(value);
}

inline icu4x::CollatorAlternateHandling icu4x::CollatorAlternateHandling::FromFFI(icu4x::capi::CollatorAlternateHandling c_enum) {
    switch (c_enum) {
        case icu4x::capi::CollatorAlternateHandling_NonIgnorable:
        case icu4x::capi::CollatorAlternateHandling_Shifted:
            return static_cast<icu4x::CollatorAlternateHandling::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CollatorAlternateHandling_HPP
