#ifndef temporal_rs_PartialDate_HPP
#define temporal_rs_PartialDate_HPP

#include "PartialDate.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "Calendar.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::PartialDate temporal_rs::PartialDate::AsFFI() const {
  return temporal_rs::capi::PartialDate {
    /* .year = */ year.has_value() ? (diplomat::capi::OptionI32{ { year.value() }, true }) : (diplomat::capi::OptionI32{ {}, false }),
    /* .month = */ month.has_value() ? (diplomat::capi::OptionU8{ { month.value() }, true }) : (diplomat::capi::OptionU8{ {}, false }),
    /* .month_code = */ {month_code.data(), month_code.size()},
    /* .day = */ day.has_value() ? (diplomat::capi::OptionU8{ { day.value() }, true }) : (diplomat::capi::OptionU8{ {}, false }),
    /* .era = */ {era.data(), era.size()},
    /* .era_year = */ era_year.has_value() ? (diplomat::capi::OptionI32{ { era_year.value() }, true }) : (diplomat::capi::OptionI32{ {}, false }),
    /* .calendar = */ calendar.AsFFI(),
  };
}

inline temporal_rs::PartialDate temporal_rs::PartialDate::FromFFI(temporal_rs::capi::PartialDate c_struct) {
  return temporal_rs::PartialDate {
    /* .year = */ c_struct.year.is_ok ? std::optional(c_struct.year.ok) : std::nullopt,
    /* .month = */ c_struct.month.is_ok ? std::optional(c_struct.month.ok) : std::nullopt,
    /* .month_code = */ std::string_view(c_struct.month_code.data, c_struct.month_code.len),
    /* .day = */ c_struct.day.is_ok ? std::optional(c_struct.day.ok) : std::nullopt,
    /* .era = */ std::string_view(c_struct.era.data, c_struct.era.len),
    /* .era_year = */ c_struct.era_year.is_ok ? std::optional(c_struct.era_year.ok) : std::nullopt,
    /* .calendar = */ *temporal_rs::Calendar::FromFFI(c_struct.calendar),
  };
}


#endif // temporal_rs_PartialDate_HPP
