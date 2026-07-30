#ifndef ICU4X_TimeZoneVariant_HPP
#define ICU4X_TimeZoneVariant_HPP

#include "TimeZoneVariant.d.hpp"

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
    extern "C" {

    icu4x::capi::TimeZoneVariant icu4x_TimeZoneVariant_from_rearguard_isdst_mv1(bool isdst);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::TimeZoneVariant icu4x::TimeZoneVariant::AsFFI() const {
    return static_cast<icu4x::capi::TimeZoneVariant>(value);
}

inline icu4x::TimeZoneVariant icu4x::TimeZoneVariant::FromFFI(icu4x::capi::TimeZoneVariant c_enum) {
    switch (c_enum) {
        case icu4x::capi::TimeZoneVariant_Standard:
        case icu4x::capi::TimeZoneVariant_Daylight:
            return static_cast<icu4x::TimeZoneVariant::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::TimeZoneVariant icu4x::TimeZoneVariant::from_rearguard_isdst(bool isdst) {
    auto result = icu4x::capi::icu4x_TimeZoneVariant_from_rearguard_isdst_mv1(isdst);
    return icu4x::TimeZoneVariant::FromFFI(result);
}
#endif // ICU4X_TimeZoneVariant_HPP
