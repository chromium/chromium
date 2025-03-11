#ifndef icu4x_ZonedIsoDateTime_HPP
#define icu4x_ZonedIsoDateTime_HPP

#include "ZonedIsoDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "CalendarParseError.hpp"
#include "IanaParser.hpp"
#include "IsoDate.hpp"
#include "Time.hpp"
#include "TimeZoneInfo.hpp"
#include "UtcOffsetCalculator.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_ZonedIsoDateTime_from_string_mv1_result {union {icu4x::capi::ZonedIsoDateTime ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_ZonedIsoDateTime_from_string_mv1_result;
    icu4x_ZonedIsoDateTime_from_string_mv1_result icu4x_ZonedIsoDateTime_from_string_mv1(diplomat::capi::DiplomatStringView v, const icu4x::capi::IanaParser* iana_parser, const icu4x::capi::UtcOffsetCalculator* offset_calculator);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<icu4x::ZonedIsoDateTime, icu4x::CalendarParseError> icu4x::ZonedIsoDateTime::from_string(std::string_view v, const icu4x::IanaParser& iana_parser, const icu4x::UtcOffsetCalculator& offset_calculator) {
  auto result = icu4x::capi::icu4x_ZonedIsoDateTime_from_string_mv1({v.data(), v.size()},
    iana_parser.AsFFI(),
    offset_calculator.AsFFI());
  return result.is_ok ? diplomat::result<icu4x::ZonedIsoDateTime, icu4x::CalendarParseError>(diplomat::Ok<icu4x::ZonedIsoDateTime>(icu4x::ZonedIsoDateTime::FromFFI(result.ok))) : diplomat::result<icu4x::ZonedIsoDateTime, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}


inline icu4x::capi::ZonedIsoDateTime icu4x::ZonedIsoDateTime::AsFFI() const {
  return icu4x::capi::ZonedIsoDateTime {
    /* .date = */ date->AsFFI(),
    /* .time = */ time->AsFFI(),
    /* .zone = */ zone->AsFFI(),
  };
}

inline icu4x::ZonedIsoDateTime icu4x::ZonedIsoDateTime::FromFFI(icu4x::capi::ZonedIsoDateTime c_struct) {
  return icu4x::ZonedIsoDateTime {
    /* .date = */ std::unique_ptr<icu4x::IsoDate>(icu4x::IsoDate::FromFFI(c_struct.date)),
    /* .time = */ std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(c_struct.time)),
    /* .zone = */ std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(c_struct.zone)),
  };
}


#endif // icu4x_ZonedIsoDateTime_HPP
