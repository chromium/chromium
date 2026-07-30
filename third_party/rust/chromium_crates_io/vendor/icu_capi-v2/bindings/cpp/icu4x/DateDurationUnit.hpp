#ifndef ICU4X_DateDurationUnit_HPP
#define ICU4X_DateDurationUnit_HPP

#include "DateDurationUnit.d.hpp"

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

inline icu4x::capi::DateDurationUnit icu4x::DateDurationUnit::AsFFI() const {
    return static_cast<icu4x::capi::DateDurationUnit>(value);
}

inline icu4x::DateDurationUnit icu4x::DateDurationUnit::FromFFI(icu4x::capi::DateDurationUnit c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateDurationUnit_Years:
        case icu4x::capi::DateDurationUnit_Months:
        case icu4x::capi::DateDurationUnit_Weeks:
        case icu4x::capi::DateDurationUnit_Days:
            return static_cast<icu4x::DateDurationUnit::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateDurationUnit_HPP
