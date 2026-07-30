#ifndef ICU4X_LocaleDirection_HPP
#define ICU4X_LocaleDirection_HPP

#include "LocaleDirection.d.hpp"

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

inline icu4x::capi::LocaleDirection icu4x::LocaleDirection::AsFFI() const {
    return static_cast<icu4x::capi::LocaleDirection>(value);
}

inline icu4x::LocaleDirection icu4x::LocaleDirection::FromFFI(icu4x::capi::LocaleDirection c_enum) {
    switch (c_enum) {
        case icu4x::capi::LocaleDirection_LeftToRight:
        case icu4x::capi::LocaleDirection_RightToLeft:
        case icu4x::capi::LocaleDirection_Unknown:
            return static_cast<icu4x::LocaleDirection::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_LocaleDirection_HPP
