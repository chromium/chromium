#ifndef ICU4X_DateDifferenceOptions_HPP
#define ICU4X_DateDifferenceOptions_HPP

#include "DateDifferenceOptions.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DateDurationUnit.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::DateDifferenceOptions icu4x::DateDifferenceOptions::AsFFI() const {
    return icu4x::capi::DateDifferenceOptions {
        /* .largest_unit = */ largest_unit.has_value() ? (icu4x::capi::DateDurationUnit_option{ { largest_unit.value().AsFFI() }, true }) : (icu4x::capi::DateDurationUnit_option{ {}, false }),
    };
}

inline icu4x::DateDifferenceOptions icu4x::DateDifferenceOptions::FromFFI(icu4x::capi::DateDifferenceOptions c_struct) {
    return icu4x::DateDifferenceOptions {
        /* .largest_unit = */ c_struct.largest_unit.is_ok ? std::optional(icu4x::DateDurationUnit::FromFFI(c_struct.largest_unit.ok)) : std::nullopt,
    };
}


#endif // ICU4X_DateDifferenceOptions_HPP
