#ifndef icu4x_DateTime_HPP
#define icu4x_DateTime_HPP

#include "DateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "Calendar.hpp"
#include "CalendarParseError.hpp"
#include "Date.hpp"
#include "Time.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_DateTime_from_string_mv1_result {union {icu4x::capi::DateTime ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_DateTime_from_string_mv1_result;
    icu4x_DateTime_from_string_mv1_result icu4x_DateTime_from_string_mv1(diplomat::capi::DiplomatStringView v, const icu4x::capi::Calendar* calendar);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<icu4x::DateTime, icu4x::CalendarParseError> icu4x::DateTime::from_string(std::string_view v, const icu4x::Calendar& calendar) {
  auto result = icu4x::capi::icu4x_DateTime_from_string_mv1({v.data(), v.size()},
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<icu4x::DateTime, icu4x::CalendarParseError>(diplomat::Ok<icu4x::DateTime>(icu4x::DateTime::FromFFI(result.ok))) : diplomat::result<icu4x::DateTime, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}


inline icu4x::capi::DateTime icu4x::DateTime::AsFFI() const {
  return icu4x::capi::DateTime {
    /* .date = */ date->AsFFI(),
    /* .time = */ time->AsFFI(),
  };
}

inline icu4x::DateTime icu4x::DateTime::FromFFI(icu4x::capi::DateTime c_struct) {
  return icu4x::DateTime {
    /* .date = */ std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(c_struct.date)),
    /* .time = */ std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(c_struct.time)),
  };
}


#endif // icu4x_DateTime_HPP
