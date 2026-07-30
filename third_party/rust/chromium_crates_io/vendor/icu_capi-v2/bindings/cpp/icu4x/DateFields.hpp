#ifndef ICU4X_DateFields_HPP
#define ICU4X_DateFields_HPP

#include "DateFields.d.hpp"

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


inline icu4x::capi::DateFields icu4x::DateFields::AsFFI() const {
    return icu4x::capi::DateFields {
        /* .era = */ era.has_value() ? (icu4x::diplomat::capi::OptionStringView{ { {era.value().data(), era.value().size()} }, true }) : (icu4x::diplomat::capi::OptionStringView{ {}, false }),
        /* .era_year = */ era_year.has_value() ? (icu4x::diplomat::capi::OptionI32{ { era_year.value() }, true }) : (icu4x::diplomat::capi::OptionI32{ {}, false }),
        /* .extended_year = */ extended_year.has_value() ? (icu4x::diplomat::capi::OptionI32{ { extended_year.value() }, true }) : (icu4x::diplomat::capi::OptionI32{ {}, false }),
        /* .month_code = */ month_code.has_value() ? (icu4x::diplomat::capi::OptionStringView{ { {month_code.value().data(), month_code.value().size()} }, true }) : (icu4x::diplomat::capi::OptionStringView{ {}, false }),
        /* .ordinal_month = */ ordinal_month.has_value() ? (icu4x::diplomat::capi::OptionU8{ { ordinal_month.value() }, true }) : (icu4x::diplomat::capi::OptionU8{ {}, false }),
        /* .day = */ day.has_value() ? (icu4x::diplomat::capi::OptionU8{ { day.value() }, true }) : (icu4x::diplomat::capi::OptionU8{ {}, false }),
    };
}

inline icu4x::DateFields icu4x::DateFields::FromFFI(icu4x::capi::DateFields c_struct) {
    return icu4x::DateFields {
        /* .era = */ c_struct.era.is_ok ? std::optional(std::string_view(c_struct.era.ok.data, c_struct.era.ok.len)) : std::nullopt,
        /* .era_year = */ c_struct.era_year.is_ok ? std::optional(c_struct.era_year.ok) : std::nullopt,
        /* .extended_year = */ c_struct.extended_year.is_ok ? std::optional(c_struct.extended_year.ok) : std::nullopt,
        /* .month_code = */ c_struct.month_code.is_ok ? std::optional(std::string_view(c_struct.month_code.ok.data, c_struct.month_code.ok.len)) : std::nullopt,
        /* .ordinal_month = */ c_struct.ordinal_month.is_ok ? std::optional(c_struct.ordinal_month.ok) : std::nullopt,
        /* .day = */ c_struct.day.is_ok ? std::optional(c_struct.day.ok) : std::nullopt,
    };
}


#endif // ICU4X_DateFields_HPP
