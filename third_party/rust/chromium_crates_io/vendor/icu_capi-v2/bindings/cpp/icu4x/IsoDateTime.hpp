#ifndef icu4x_IsoDateTime_HPP
#define icu4x_IsoDateTime_HPP

#include "IsoDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "CalendarParseError.hpp"
#include "IsoDate.hpp"
#include "Time.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_IsoDateTime_from_string_mv1_result {union {icu4x::capi::IsoDateTime ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_IsoDateTime_from_string_mv1_result;
    icu4x_IsoDateTime_from_string_mv1_result icu4x_IsoDateTime_from_string_mv1(diplomat::capi::DiplomatStringView v);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<icu4x::IsoDateTime, icu4x::CalendarParseError> icu4x::IsoDateTime::from_string(std::string_view v) {
  auto result = icu4x::capi::icu4x_IsoDateTime_from_string_mv1({v.data(), v.size()});
  return result.is_ok ? diplomat::result<icu4x::IsoDateTime, icu4x::CalendarParseError>(diplomat::Ok<icu4x::IsoDateTime>(icu4x::IsoDateTime::FromFFI(result.ok))) : diplomat::result<icu4x::IsoDateTime, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}


inline icu4x::capi::IsoDateTime icu4x::IsoDateTime::AsFFI() const {
  return icu4x::capi::IsoDateTime {
    /* .date = */ date->AsFFI(),
    /* .time = */ time->AsFFI(),
  };
}

inline icu4x::IsoDateTime icu4x::IsoDateTime::FromFFI(icu4x::capi::IsoDateTime c_struct) {
  return icu4x::IsoDateTime {
    /* .date = */ std::unique_ptr<icu4x::IsoDate>(icu4x::IsoDate::FromFFI(c_struct.date)),
    /* .time = */ std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(c_struct.time)),
  };
}


#endif // icu4x_IsoDateTime_HPP
