#ifndef ICU4X_YearStyle_HPP
#define ICU4X_YearStyle_HPP

#include "YearStyle.d.hpp"

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

inline icu4x::capi::YearStyle icu4x::YearStyle::AsFFI() const {
    return static_cast<icu4x::capi::YearStyle>(value);
}

inline icu4x::YearStyle icu4x::YearStyle::FromFFI(icu4x::capi::YearStyle c_enum) {
    switch (c_enum) {
        case icu4x::capi::YearStyle_Auto:
        case icu4x::capi::YearStyle_Full:
        case icu4x::capi::YearStyle_WithEra:
        case icu4x::capi::YearStyle_NoEra:
            return static_cast<icu4x::YearStyle::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_YearStyle_HPP
