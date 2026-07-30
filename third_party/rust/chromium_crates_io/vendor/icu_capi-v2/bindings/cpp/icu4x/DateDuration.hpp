#ifndef ICU4X_DateDuration_HPP
#define ICU4X_DateDuration_HPP

#include "DateDuration.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DateDurationParseError.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_DateDuration_from_string_mv1_result {union {icu4x::capi::DateDuration ok; icu4x::capi::DateDurationParseError err;}; bool is_ok;} icu4x_DateDuration_from_string_mv1_result;
    icu4x_DateDuration_from_string_mv1_result icu4x_DateDuration_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v);

    icu4x::capi::DateDuration icu4x_DateDuration_for_years_mv1(int32_t years);

    icu4x::capi::DateDuration icu4x_DateDuration_for_months_mv1(int32_t months);

    icu4x::capi::DateDuration icu4x_DateDuration_for_weeks_mv1(int32_t weeks);

    icu4x::capi::DateDuration icu4x_DateDuration_for_days_mv1(int32_t days);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<icu4x::DateDuration, icu4x::DateDurationParseError> icu4x::DateDuration::from_string(std::string_view v) {
    auto result = icu4x::capi::icu4x_DateDuration_from_string_mv1({v.data(), v.size()});
    return result.is_ok ? icu4x::diplomat::result<icu4x::DateDuration, icu4x::DateDurationParseError>(icu4x::diplomat::Ok<icu4x::DateDuration>(icu4x::DateDuration::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::DateDuration, icu4x::DateDurationParseError>(icu4x::diplomat::Err<icu4x::DateDurationParseError>(icu4x::DateDurationParseError::FromFFI(result.err)));
}

inline icu4x::DateDuration icu4x::DateDuration::for_years(int32_t years) {
    auto result = icu4x::capi::icu4x_DateDuration_for_years_mv1(years);
    return icu4x::DateDuration::FromFFI(result);
}

inline icu4x::DateDuration icu4x::DateDuration::for_months(int32_t months) {
    auto result = icu4x::capi::icu4x_DateDuration_for_months_mv1(months);
    return icu4x::DateDuration::FromFFI(result);
}

inline icu4x::DateDuration icu4x::DateDuration::for_weeks(int32_t weeks) {
    auto result = icu4x::capi::icu4x_DateDuration_for_weeks_mv1(weeks);
    return icu4x::DateDuration::FromFFI(result);
}

inline icu4x::DateDuration icu4x::DateDuration::for_days(int32_t days) {
    auto result = icu4x::capi::icu4x_DateDuration_for_days_mv1(days);
    return icu4x::DateDuration::FromFFI(result);
}


inline icu4x::capi::DateDuration icu4x::DateDuration::AsFFI() const {
    return icu4x::capi::DateDuration {
        /* .is_negative = */ is_negative,
        /* .years = */ years,
        /* .months = */ months,
        /* .weeks = */ weeks,
        /* .days = */ days,
    };
}

inline icu4x::DateDuration icu4x::DateDuration::FromFFI(icu4x::capi::DateDuration c_struct) {
    return icu4x::DateDuration {
        /* .is_negative = */ c_struct.is_negative,
        /* .years = */ c_struct.years,
        /* .months = */ c_struct.months,
        /* .weeks = */ c_struct.weeks,
        /* .days = */ c_struct.days,
    };
}


#endif // ICU4X_DateDuration_HPP
