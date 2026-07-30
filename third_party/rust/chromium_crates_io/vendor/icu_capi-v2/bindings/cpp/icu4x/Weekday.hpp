#ifndef ICU4X_Weekday_HPP
#define ICU4X_Weekday_HPP

#include "Weekday.d.hpp"

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

inline icu4x::capi::Weekday icu4x::Weekday::AsFFI() const {
    return static_cast<icu4x::capi::Weekday>(value);
}

inline icu4x::Weekday icu4x::Weekday::FromFFI(icu4x::capi::Weekday c_enum) {
    switch (c_enum) {
        case icu4x::capi::Weekday_Monday:
        case icu4x::capi::Weekday_Tuesday:
        case icu4x::capi::Weekday_Wednesday:
        case icu4x::capi::Weekday_Thursday:
        case icu4x::capi::Weekday_Friday:
        case icu4x::capi::Weekday_Saturday:
        case icu4x::capi::Weekday_Sunday:
            return static_cast<icu4x::Weekday::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_Weekday_HPP
