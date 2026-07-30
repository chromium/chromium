#ifndef ICU4X_IsoWeekOfYear_HPP
#define ICU4X_IsoWeekOfYear_HPP

#include "IsoWeekOfYear.d.hpp"

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


inline icu4x::capi::IsoWeekOfYear icu4x::IsoWeekOfYear::AsFFI() const {
    return icu4x::capi::IsoWeekOfYear {
        /* .week_number = */ week_number,
        /* .iso_year = */ iso_year,
    };
}

inline icu4x::IsoWeekOfYear icu4x::IsoWeekOfYear::FromFFI(icu4x::capi::IsoWeekOfYear c_struct) {
    return icu4x::IsoWeekOfYear {
        /* .week_number = */ c_struct.week_number,
        /* .iso_year = */ c_struct.iso_year,
    };
}


#endif // ICU4X_IsoWeekOfYear_HPP
