#ifndef ICU4X_ZonedIsoDateTime_HPP
#define ICU4X_ZonedIsoDateTime_HPP

#include "ZonedIsoDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "IanaParser.hpp"
#include "IsoDate.hpp"
#include "Rfc9557ParseError.hpp"
#include "Time.hpp"
#include "TimeZoneInfo.hpp"
#include "UtcOffset.hpp"
#include "VariantOffsetsCalculator.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_ZonedIsoDateTime_strict_from_string_mv1_result {union {icu4x::capi::ZonedIsoDateTime ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedIsoDateTime_strict_from_string_mv1_result;
    icu4x_ZonedIsoDateTime_strict_from_string_mv1_result icu4x_ZonedIsoDateTime_strict_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v, const icu4x::capi::IanaParser* iana_parser);

    typedef struct icu4x_ZonedIsoDateTime_full_from_string_mv1_result {union {icu4x::capi::ZonedIsoDateTime ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedIsoDateTime_full_from_string_mv1_result;
    icu4x_ZonedIsoDateTime_full_from_string_mv1_result icu4x_ZonedIsoDateTime_full_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v, const icu4x::capi::IanaParser* iana_parser, const icu4x::capi::VariantOffsetsCalculator* _offset_calculator);

    icu4x::capi::ZonedIsoDateTime icu4x_ZonedIsoDateTime_from_epoch_milliseconds_and_utc_offset_mv1(int64_t epoch_milliseconds, const icu4x::capi::UtcOffset* utc_offset);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError> icu4x::ZonedIsoDateTime::strict_from_string(std::string_view v, const icu4x::IanaParser& iana_parser) {
    auto result = icu4x::capi::icu4x_ZonedIsoDateTime_strict_from_string_mv1({v.data(), v.size()},
        iana_parser.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<icu4x::ZonedIsoDateTime>(icu4x::ZonedIsoDateTime::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError> icu4x::ZonedIsoDateTime::full_from_string(std::string_view v, const icu4x::IanaParser& iana_parser, const icu4x::VariantOffsetsCalculator& _offset_calculator) {
    auto result = icu4x::capi::icu4x_ZonedIsoDateTime_full_from_string_mv1({v.data(), v.size()},
        iana_parser.AsFFI(),
        _offset_calculator.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<icu4x::ZonedIsoDateTime>(icu4x::ZonedIsoDateTime::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}

inline icu4x::ZonedIsoDateTime icu4x::ZonedIsoDateTime::from_epoch_milliseconds_and_utc_offset(int64_t epoch_milliseconds, const icu4x::UtcOffset& utc_offset) {
    auto result = icu4x::capi::icu4x_ZonedIsoDateTime_from_epoch_milliseconds_and_utc_offset_mv1(epoch_milliseconds,
        utc_offset.AsFFI());
    return icu4x::ZonedIsoDateTime::FromFFI(result);
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


#endif // ICU4X_ZonedIsoDateTime_HPP
