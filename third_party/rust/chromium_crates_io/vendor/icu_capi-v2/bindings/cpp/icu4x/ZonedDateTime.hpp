#ifndef icu4x_ZonedDateTime_HPP
#define icu4x_ZonedDateTime_HPP

#include "ZonedDateTime.d.hpp"

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
#include "IanaParser.hpp"
#include "Time.hpp"
#include "TimeZoneInfo.hpp"
#include "UtcOffsetCalculator.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_ZonedDateTime_from_string_mv1_result {union {icu4x::capi::ZonedDateTime ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_ZonedDateTime_from_string_mv1_result;
    icu4x_ZonedDateTime_from_string_mv1_result icu4x_ZonedDateTime_from_string_mv1(diplomat::capi::DiplomatStringView v, const icu4x::capi::Calendar* calendar, const icu4x::capi::IanaParser* iana_parser, const icu4x::capi::UtcOffsetCalculator* offset_calculator);
    
    typedef struct icu4x_ZonedDateTime_location_only_from_string_mv1_result {union {icu4x::capi::ZonedDateTime ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_ZonedDateTime_location_only_from_string_mv1_result;
    icu4x_ZonedDateTime_location_only_from_string_mv1_result icu4x_ZonedDateTime_location_only_from_string_mv1(diplomat::capi::DiplomatStringView v, const icu4x::capi::Calendar* calendar, const icu4x::capi::IanaParser* iana_parser);
    
    typedef struct icu4x_ZonedDateTime_offset_only_from_string_mv1_result {union {icu4x::capi::ZonedDateTime ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_ZonedDateTime_offset_only_from_string_mv1_result;
    icu4x_ZonedDateTime_offset_only_from_string_mv1_result icu4x_ZonedDateTime_offset_only_from_string_mv1(diplomat::capi::DiplomatStringView v, const icu4x::capi::Calendar* calendar);
    
    typedef struct icu4x_ZonedDateTime_loose_from_string_mv1_result {union {icu4x::capi::ZonedDateTime ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_ZonedDateTime_loose_from_string_mv1_result;
    icu4x_ZonedDateTime_loose_from_string_mv1_result icu4x_ZonedDateTime_loose_from_string_mv1(diplomat::capi::DiplomatStringView v, const icu4x::capi::Calendar* calendar, const icu4x::capi::IanaParser* iana_parser);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError> icu4x::ZonedDateTime::from_string(std::string_view v, const icu4x::Calendar& calendar, const icu4x::IanaParser& iana_parser, const icu4x::UtcOffsetCalculator& offset_calculator) {
  auto result = icu4x::capi::icu4x_ZonedDateTime_from_string_mv1({v.data(), v.size()},
    calendar.AsFFI(),
    iana_parser.AsFFI(),
    offset_calculator.AsFFI());
  return result.is_ok ? diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Ok<icu4x::ZonedDateTime>(icu4x::ZonedDateTime::FromFFI(result.ok))) : diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}

inline diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError> icu4x::ZonedDateTime::location_only_from_string(std::string_view v, const icu4x::Calendar& calendar, const icu4x::IanaParser& iana_parser) {
  auto result = icu4x::capi::icu4x_ZonedDateTime_location_only_from_string_mv1({v.data(), v.size()},
    calendar.AsFFI(),
    iana_parser.AsFFI());
  return result.is_ok ? diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Ok<icu4x::ZonedDateTime>(icu4x::ZonedDateTime::FromFFI(result.ok))) : diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}

inline diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError> icu4x::ZonedDateTime::offset_only_from_string(std::string_view v, const icu4x::Calendar& calendar) {
  auto result = icu4x::capi::icu4x_ZonedDateTime_offset_only_from_string_mv1({v.data(), v.size()},
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Ok<icu4x::ZonedDateTime>(icu4x::ZonedDateTime::FromFFI(result.ok))) : diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}

inline diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError> icu4x::ZonedDateTime::loose_from_string(std::string_view v, const icu4x::Calendar& calendar, const icu4x::IanaParser& iana_parser) {
  auto result = icu4x::capi::icu4x_ZonedDateTime_loose_from_string_mv1({v.data(), v.size()},
    calendar.AsFFI(),
    iana_parser.AsFFI());
  return result.is_ok ? diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Ok<icu4x::ZonedDateTime>(icu4x::ZonedDateTime::FromFFI(result.ok))) : diplomat::result<icu4x::ZonedDateTime, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}


inline icu4x::capi::ZonedDateTime icu4x::ZonedDateTime::AsFFI() const {
  return icu4x::capi::ZonedDateTime {
    /* .date = */ date->AsFFI(),
    /* .time = */ time->AsFFI(),
    /* .zone = */ zone->AsFFI(),
  };
}

inline icu4x::ZonedDateTime icu4x::ZonedDateTime::FromFFI(icu4x::capi::ZonedDateTime c_struct) {
  return icu4x::ZonedDateTime {
    /* .date = */ std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(c_struct.date)),
    /* .time = */ std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(c_struct.time)),
    /* .zone = */ std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(c_struct.zone)),
  };
}


#endif // icu4x_ZonedDateTime_HPP
