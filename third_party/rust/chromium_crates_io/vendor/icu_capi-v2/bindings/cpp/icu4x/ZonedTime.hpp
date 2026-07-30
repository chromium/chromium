#ifndef ICU4X_ZonedTime_HPP
#define ICU4X_ZonedTime_HPP

#include "ZonedTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "IanaParser.hpp"
#include "Rfc9557ParseError.hpp"
#include "Time.hpp"
#include "TimeZoneInfo.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_ZonedTime_strict_from_string_mv1_result {union {icu4x::capi::ZonedTime ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_strict_from_string_mv1_result;
    icu4x_ZonedTime_strict_from_string_mv1_result icu4x_ZonedTime_strict_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v, const icu4x::capi::IanaParser* iana_parser);

    typedef struct icu4x_ZonedTime_location_only_from_string_mv1_result {union {icu4x::capi::ZonedTime ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_location_only_from_string_mv1_result;
    icu4x_ZonedTime_location_only_from_string_mv1_result icu4x_ZonedTime_location_only_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v, const icu4x::capi::IanaParser* iana_parser);

    typedef struct icu4x_ZonedTime_offset_only_from_string_mv1_result {union {icu4x::capi::ZonedTime ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_offset_only_from_string_mv1_result;
    icu4x_ZonedTime_offset_only_from_string_mv1_result icu4x_ZonedTime_offset_only_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v);

    typedef struct icu4x_ZonedTime_lenient_from_string_mv1_result {union {icu4x::capi::ZonedTime ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_lenient_from_string_mv1_result;
    icu4x_ZonedTime_lenient_from_string_mv1_result icu4x_ZonedTime_lenient_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v, const icu4x::capi::IanaParser* iana_parser);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> icu4x::ZonedTime::strict_from_string(std::string_view v, const icu4x::IanaParser& iana_parser) {
    auto result = icu4x::capi::icu4x_ZonedTime_strict_from_string_mv1({v.data(), v.size()},
        iana_parser.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<icu4x::ZonedTime>(icu4x::ZonedTime::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> icu4x::ZonedTime::location_only_from_string(std::string_view v, const icu4x::IanaParser& iana_parser) {
    auto result = icu4x::capi::icu4x_ZonedTime_location_only_from_string_mv1({v.data(), v.size()},
        iana_parser.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<icu4x::ZonedTime>(icu4x::ZonedTime::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> icu4x::ZonedTime::offset_only_from_string(std::string_view v) {
    auto result = icu4x::capi::icu4x_ZonedTime_offset_only_from_string_mv1({v.data(), v.size()});
    return result.is_ok ? icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<icu4x::ZonedTime>(icu4x::ZonedTime::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> icu4x::ZonedTime::lenient_from_string(std::string_view v, const icu4x::IanaParser& iana_parser) {
    auto result = icu4x::capi::icu4x_ZonedTime_lenient_from_string_mv1({v.data(), v.size()},
        iana_parser.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<icu4x::ZonedTime>(icu4x::ZonedTime::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}


inline icu4x::capi::ZonedTime icu4x::ZonedTime::AsFFI() const {
    return icu4x::capi::ZonedTime {
        /* .time = */ time->AsFFI(),
        /* .zone = */ zone->AsFFI(),
    };
}

inline icu4x::ZonedTime icu4x::ZonedTime::FromFFI(icu4x::capi::ZonedTime c_struct) {
    return icu4x::ZonedTime {
        /* .time = */ std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(c_struct.time)),
        /* .zone = */ std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(c_struct.zone)),
    };
}


#endif // ICU4X_ZonedTime_HPP
