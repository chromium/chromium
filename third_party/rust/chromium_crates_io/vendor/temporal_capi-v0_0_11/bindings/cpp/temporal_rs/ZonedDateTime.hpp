#ifndef temporal_rs_ZonedDateTime_HPP
#define temporal_rs_ZonedDateTime_HPP

#include "ZonedDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "AnyCalendarKind.hpp"
#include "ArithmeticOverflow.hpp"
#include "Calendar.hpp"
#include "DifferenceSettings.hpp"
#include "Disambiguation.hpp"
#include "DisplayCalendar.hpp"
#include "DisplayOffset.hpp"
#include "DisplayTimeZone.hpp"
#include "Duration.hpp"
#include "I128Nanoseconds.hpp"
#include "Instant.hpp"
#include "OffsetDisambiguation.hpp"
#include "OwnedPartialZonedDateTime.hpp"
#include "PartialZonedDateTime.hpp"
#include "PlainDate.hpp"
#include "PlainDateTime.hpp"
#include "PlainTime.hpp"
#include "RoundingOptions.hpp"
#include "TemporalError.hpp"
#include "TimeZone.hpp"
#include "ToStringRoundingOptions.hpp"
#include "TransitionDirection.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_ZonedDateTime_try_new_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_try_new_result;
    temporal_rs_ZonedDateTime_try_new_result temporal_rs_ZonedDateTime_try_new(temporal_rs::capi::I128Nanoseconds nanosecond, temporal_rs::capi::AnyCalendarKind calendar, const temporal_rs::capi::TimeZone* time_zone);

    typedef struct temporal_rs_ZonedDateTime_from_partial_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_partial_result;
    temporal_rs_ZonedDateTime_from_partial_result temporal_rs_ZonedDateTime_from_partial(temporal_rs::capi::PartialZonedDateTime partial, temporal_rs::capi::ArithmeticOverflow_option overflow, temporal_rs::capi::Disambiguation_option disambiguation, temporal_rs::capi::OffsetDisambiguation_option offset_option);

    typedef struct temporal_rs_ZonedDateTime_from_owned_partial_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_owned_partial_result;
    temporal_rs_ZonedDateTime_from_owned_partial_result temporal_rs_ZonedDateTime_from_owned_partial(const temporal_rs::capi::OwnedPartialZonedDateTime* partial, temporal_rs::capi::ArithmeticOverflow_option overflow, temporal_rs::capi::Disambiguation_option disambiguation, temporal_rs::capi::OffsetDisambiguation_option offset_option);

    typedef struct temporal_rs_ZonedDateTime_from_utf8_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf8_result;
    temporal_rs_ZonedDateTime_from_utf8_result temporal_rs_ZonedDateTime_from_utf8(diplomat::capi::DiplomatStringView s, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_disambiguation);

    typedef struct temporal_rs_ZonedDateTime_from_utf16_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf16_result;
    temporal_rs_ZonedDateTime_from_utf16_result temporal_rs_ZonedDateTime_from_utf16(diplomat::capi::DiplomatString16View s, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_disambiguation);

    int64_t temporal_rs_ZonedDateTime_epoch_milliseconds(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_from_epoch_milliseconds_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_milliseconds_result;
    temporal_rs_ZonedDateTime_from_epoch_milliseconds_result temporal_rs_ZonedDateTime_from_epoch_milliseconds(int64_t ms, const temporal_rs::capi::TimeZone* tz);

    temporal_rs::capi::I128Nanoseconds temporal_rs_ZonedDateTime_epoch_nanoseconds(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_offset_nanoseconds_result {union {int64_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_offset_nanoseconds_result;
    temporal_rs_ZonedDateTime_offset_nanoseconds_result temporal_rs_ZonedDateTime_offset_nanoseconds(const temporal_rs::capi::ZonedDateTime* self);

    temporal_rs::capi::Instant* temporal_rs_ZonedDateTime_to_instant(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_with_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_result;
    temporal_rs_ZonedDateTime_with_result temporal_rs_ZonedDateTime_with(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::PartialZonedDateTime partial, temporal_rs::capi::Disambiguation_option disambiguation, temporal_rs::capi::OffsetDisambiguation_option offset_option, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_ZonedDateTime_with_timezone_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_timezone_result;
    temporal_rs_ZonedDateTime_with_timezone_result temporal_rs_ZonedDateTime_with_timezone(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::TimeZone* zone);

    const temporal_rs::capi::TimeZone* temporal_rs_ZonedDateTime_timezone(const temporal_rs::capi::ZonedDateTime* self);

    int8_t temporal_rs_ZonedDateTime_compare_instant(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other);

    bool temporal_rs_ZonedDateTime_equals(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other);

    typedef struct temporal_rs_ZonedDateTime_offset_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_offset_result;
    temporal_rs_ZonedDateTime_offset_result temporal_rs_ZonedDateTime_offset(const temporal_rs::capi::ZonedDateTime* self, diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_ZonedDateTime_start_of_day_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_start_of_day_result;
    temporal_rs_ZonedDateTime_start_of_day_result temporal_rs_ZonedDateTime_start_of_day(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_get_time_zone_transition_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_get_time_zone_transition_result;
    temporal_rs_ZonedDateTime_get_time_zone_transition_result temporal_rs_ZonedDateTime_get_time_zone_transition(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::TransitionDirection direction);

    typedef struct temporal_rs_ZonedDateTime_hours_in_day_result {union {uint8_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_hours_in_day_result;
    temporal_rs_ZonedDateTime_hours_in_day_result temporal_rs_ZonedDateTime_hours_in_day(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_to_plain_datetime_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_plain_datetime_result;
    temporal_rs_ZonedDateTime_to_plain_datetime_result temporal_rs_ZonedDateTime_to_plain_datetime(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_to_plain_date_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_plain_date_result;
    temporal_rs_ZonedDateTime_to_plain_date_result temporal_rs_ZonedDateTime_to_plain_date(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_to_plain_time_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_plain_time_result;
    temporal_rs_ZonedDateTime_to_plain_time_result temporal_rs_ZonedDateTime_to_plain_time(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_to_ixdtf_string_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_ixdtf_string_result;
    temporal_rs_ZonedDateTime_to_ixdtf_string_result temporal_rs_ZonedDateTime_to_ixdtf_string(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::DisplayOffset display_offset, temporal_rs::capi::DisplayTimeZone display_timezone, temporal_rs::capi::DisplayCalendar display_calendar, temporal_rs::capi::ToStringRoundingOptions options, diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_ZonedDateTime_with_calendar_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_calendar_result;
    temporal_rs_ZonedDateTime_with_calendar_result temporal_rs_ZonedDateTime_with_calendar(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::AnyCalendarKind calendar);

    typedef struct temporal_rs_ZonedDateTime_with_plain_time_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_plain_time_result;
    temporal_rs_ZonedDateTime_with_plain_time_result temporal_rs_ZonedDateTime_with_plain_time(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::PlainTime* time);

    typedef struct temporal_rs_ZonedDateTime_add_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_add_result;
    temporal_rs_ZonedDateTime_add_result temporal_rs_ZonedDateTime_add(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_ZonedDateTime_subtract_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_subtract_result;
    temporal_rs_ZonedDateTime_subtract_result temporal_rs_ZonedDateTime_subtract(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_ZonedDateTime_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_until_result;
    temporal_rs_ZonedDateTime_until_result temporal_rs_ZonedDateTime_until(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_ZonedDateTime_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_since_result;
    temporal_rs_ZonedDateTime_since_result temporal_rs_ZonedDateTime_since(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_ZonedDateTime_round_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_round_result;
    temporal_rs_ZonedDateTime_round_result temporal_rs_ZonedDateTime_round(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::RoundingOptions options);

    uint8_t temporal_rs_ZonedDateTime_hour(const temporal_rs::capi::ZonedDateTime* self);

    uint8_t temporal_rs_ZonedDateTime_minute(const temporal_rs::capi::ZonedDateTime* self);

    uint8_t temporal_rs_ZonedDateTime_second(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_millisecond(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_microsecond(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_nanosecond(const temporal_rs::capi::ZonedDateTime* self);

    const temporal_rs::capi::Calendar* temporal_rs_ZonedDateTime_calendar(const temporal_rs::capi::ZonedDateTime* self);

    int32_t temporal_rs_ZonedDateTime_year(const temporal_rs::capi::ZonedDateTime* self);

    uint8_t temporal_rs_ZonedDateTime_month(const temporal_rs::capi::ZonedDateTime* self);

    void temporal_rs_ZonedDateTime_month_code(const temporal_rs::capi::ZonedDateTime* self, diplomat::capi::DiplomatWrite* write);

    uint8_t temporal_rs_ZonedDateTime_day(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_day_of_week_result {union {uint16_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_day_of_week_result;
    temporal_rs_ZonedDateTime_day_of_week_result temporal_rs_ZonedDateTime_day_of_week(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_day_of_year(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_week_of_year_result;
    temporal_rs_ZonedDateTime_week_of_year_result temporal_rs_ZonedDateTime_week_of_year(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_year_of_week_result;
    temporal_rs_ZonedDateTime_year_of_week_result temporal_rs_ZonedDateTime_year_of_week(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_days_in_week_result {union {uint16_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_days_in_week_result;
    temporal_rs_ZonedDateTime_days_in_week_result temporal_rs_ZonedDateTime_days_in_week(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_days_in_month(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_days_in_year(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_months_in_year(const temporal_rs::capi::ZonedDateTime* self);

    bool temporal_rs_ZonedDateTime_in_leap_year(const temporal_rs::capi::ZonedDateTime* self);

    void temporal_rs_ZonedDateTime_era(const temporal_rs::capi::ZonedDateTime* self, diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_ZonedDateTime_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_era_year_result;
    temporal_rs_ZonedDateTime_era_year_result temporal_rs_ZonedDateTime_era_year(const temporal_rs::capi::ZonedDateTime* self);

    void temporal_rs_ZonedDateTime_destroy(ZonedDateTime* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::try_new(temporal_rs::I128Nanoseconds nanosecond, temporal_rs::AnyCalendarKind calendar, const temporal_rs::TimeZone& time_zone) {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_try_new(nanosecond.AsFFI(),
    calendar.AsFFI(),
    time_zone.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_partial(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option) {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_partial(partial.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }),
    disambiguation.has_value() ? (temporal_rs::capi::Disambiguation_option{ { disambiguation.value().AsFFI() }, true }) : (temporal_rs::capi::Disambiguation_option{ {}, false }),
    offset_option.has_value() ? (temporal_rs::capi::OffsetDisambiguation_option{ { offset_option.value().AsFFI() }, true }) : (temporal_rs::capi::OffsetDisambiguation_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_owned_partial(const temporal_rs::OwnedPartialZonedDateTime& partial, std::optional<temporal_rs::ArithmeticOverflow> overflow, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option) {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_owned_partial(partial.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }),
    disambiguation.has_value() ? (temporal_rs::capi::Disambiguation_option{ { disambiguation.value().AsFFI() }, true }) : (temporal_rs::capi::Disambiguation_option{ {}, false }),
    offset_option.has_value() ? (temporal_rs::capi::OffsetDisambiguation_option{ { offset_option.value().AsFFI() }, true }) : (temporal_rs::capi::OffsetDisambiguation_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_utf8(std::string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation) {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_utf8({s.data(), s.size()},
    disambiguation.AsFFI(),
    offset_disambiguation.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_utf16(std::u16string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation) {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_utf16({s.data(), s.size()},
    disambiguation.AsFFI(),
    offset_disambiguation.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline int64_t temporal_rs::ZonedDateTime::epoch_milliseconds() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_epoch_milliseconds(this->AsFFI());
  return result;
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_epoch_milliseconds(int64_t ms, const temporal_rs::TimeZone& tz) {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_epoch_milliseconds(ms,
    tz.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::I128Nanoseconds temporal_rs::ZonedDateTime::epoch_nanoseconds() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_epoch_nanoseconds(this->AsFFI());
  return temporal_rs::I128Nanoseconds::FromFFI(result);
}

inline diplomat::result<int64_t, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::offset_nanoseconds() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_offset_nanoseconds(this->AsFFI());
  return result.is_ok ? diplomat::result<int64_t, temporal_rs::TemporalError>(diplomat::Ok<int64_t>(result.ok)) : diplomat::result<int64_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::Instant> temporal_rs::ZonedDateTime::to_instant() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_instant(this->AsFFI());
  return std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with(this->AsFFI(),
    partial.AsFFI(),
    disambiguation.has_value() ? (temporal_rs::capi::Disambiguation_option{ { disambiguation.value().AsFFI() }, true }) : (temporal_rs::capi::Disambiguation_option{ {}, false }),
    offset_option.has_value() ? (temporal_rs::capi::OffsetDisambiguation_option{ { offset_option.value().AsFFI() }, true }) : (temporal_rs::capi::OffsetDisambiguation_option{ {}, false }),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_timezone(const temporal_rs::TimeZone& zone) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_timezone(this->AsFFI(),
    zone.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::TimeZone& temporal_rs::ZonedDateTime::timezone() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_timezone(this->AsFFI());
  return *temporal_rs::TimeZone::FromFFI(result);
}

inline int8_t temporal_rs::ZonedDateTime::compare_instant(const temporal_rs::ZonedDateTime& other) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_compare_instant(this->AsFFI(),
    other.AsFFI());
  return result;
}

inline bool temporal_rs::ZonedDateTime::equals(const temporal_rs::ZonedDateTime& other) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_equals(this->AsFFI(),
    other.AsFFI());
  return result;
}

inline diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::offset() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_offset(this->AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::offset_write(W& writeable) const {
  diplomat::capi::DiplomatWrite write = diplomat::WriteTrait<W>::Construct(writeable);
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_offset(this->AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::monostate, temporal_rs::TemporalError>(diplomat::Ok<std::monostate>()) : diplomat::result<std::monostate, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::start_of_day() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_start_of_day(this->AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::get_time_zone_transition(temporal_rs::TransitionDirection direction) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_get_time_zone_transition(this->AsFFI(),
    direction.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<uint8_t, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::hours_in_day() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_hours_in_day(this->AsFFI());
  return result.is_ok ? diplomat::result<uint8_t, temporal_rs::TemporalError>(diplomat::Ok<uint8_t>(result.ok)) : diplomat::result<uint8_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_plain_datetime() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_plain_datetime(this->AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_plain_date() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_plain_date(this->AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_plain_time() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_plain_time(this->AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_ixdtf_string(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_ixdtf_string(this->AsFFI(),
    display_offset.AsFFI(),
    display_timezone.AsFFI(),
    display_calendar.AsFFI(),
    options.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_ixdtf_string_write(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options, W& writeable) const {
  diplomat::capi::DiplomatWrite write = diplomat::WriteTrait<W>::Construct(writeable);
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_ixdtf_string(this->AsFFI(),
    display_offset.AsFFI(),
    display_timezone.AsFFI(),
    display_calendar.AsFFI(),
    options.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::monostate, temporal_rs::TemporalError>(diplomat::Ok<std::monostate>()) : diplomat::result<std::monostate, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_calendar(temporal_rs::AnyCalendarKind calendar) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_calendar(this->AsFFI(),
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_plain_time(const temporal_rs::PlainTime* time) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_plain_time(this->AsFFI(),
    time ? time->AsFFI() : nullptr);
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::add(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_add(this->AsFFI(),
    duration.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::subtract(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_subtract(this->AsFFI(),
    duration.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::until(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_until(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::since(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_since(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::round(temporal_rs::RoundingOptions options) const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_round(this->AsFFI(),
    options.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint8_t temporal_rs::ZonedDateTime::hour() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_hour(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::ZonedDateTime::minute() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_minute(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::ZonedDateTime::second() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_second(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::ZonedDateTime::millisecond() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_millisecond(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::ZonedDateTime::microsecond() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_microsecond(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::ZonedDateTime::nanosecond() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_nanosecond(this->AsFFI());
  return result;
}

inline const temporal_rs::Calendar& temporal_rs::ZonedDateTime::calendar() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_calendar(this->AsFFI());
  return *temporal_rs::Calendar::FromFFI(result);
}

inline int32_t temporal_rs::ZonedDateTime::year() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_year(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::ZonedDateTime::month() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_month(this->AsFFI());
  return result;
}

inline std::string temporal_rs::ZonedDateTime::month_code() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  temporal_rs::capi::temporal_rs_ZonedDateTime_month_code(this->AsFFI(),
    &write);
  return output;
}
template<typename W>
inline void temporal_rs::ZonedDateTime::month_code_write(W& writeable) const {
  diplomat::capi::DiplomatWrite write = diplomat::WriteTrait<W>::Construct(writeable);
  temporal_rs::capi::temporal_rs_ZonedDateTime_month_code(this->AsFFI(),
    &write);
}

inline uint8_t temporal_rs::ZonedDateTime::day() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_day(this->AsFFI());
  return result;
}

inline diplomat::result<uint16_t, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::day_of_week() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_day_of_week(this->AsFFI());
  return result.is_ok ? diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Ok<uint16_t>(result.ok)) : diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint16_t temporal_rs::ZonedDateTime::day_of_year() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_day_of_year(this->AsFFI());
  return result;
}

inline std::optional<uint8_t> temporal_rs::ZonedDateTime::week_of_year() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_week_of_year(this->AsFFI());
  return result.is_ok ? std::optional<uint8_t>(result.ok) : std::nullopt;
}

inline std::optional<int32_t> temporal_rs::ZonedDateTime::year_of_week() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_year_of_week(this->AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline diplomat::result<uint16_t, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::days_in_week() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_days_in_week(this->AsFFI());
  return result.is_ok ? diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Ok<uint16_t>(result.ok)) : diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint16_t temporal_rs::ZonedDateTime::days_in_month() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_days_in_month(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::ZonedDateTime::days_in_year() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_days_in_year(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::ZonedDateTime::months_in_year() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_months_in_year(this->AsFFI());
  return result;
}

inline bool temporal_rs::ZonedDateTime::in_leap_year() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_in_leap_year(this->AsFFI());
  return result;
}

inline std::string temporal_rs::ZonedDateTime::era() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  temporal_rs::capi::temporal_rs_ZonedDateTime_era(this->AsFFI(),
    &write);
  return output;
}
template<typename W>
inline void temporal_rs::ZonedDateTime::era_write(W& writeable) const {
  diplomat::capi::DiplomatWrite write = diplomat::WriteTrait<W>::Construct(writeable);
  temporal_rs::capi::temporal_rs_ZonedDateTime_era(this->AsFFI(),
    &write);
}

inline std::optional<int32_t> temporal_rs::ZonedDateTime::era_year() const {
  auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_era_year(this->AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline const temporal_rs::capi::ZonedDateTime* temporal_rs::ZonedDateTime::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::ZonedDateTime*>(this);
}

inline temporal_rs::capi::ZonedDateTime* temporal_rs::ZonedDateTime::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::ZonedDateTime*>(this);
}

inline const temporal_rs::ZonedDateTime* temporal_rs::ZonedDateTime::FromFFI(const temporal_rs::capi::ZonedDateTime* ptr) {
  return reinterpret_cast<const temporal_rs::ZonedDateTime*>(ptr);
}

inline temporal_rs::ZonedDateTime* temporal_rs::ZonedDateTime::FromFFI(temporal_rs::capi::ZonedDateTime* ptr) {
  return reinterpret_cast<temporal_rs::ZonedDateTime*>(ptr);
}

inline void temporal_rs::ZonedDateTime::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_ZonedDateTime_destroy(reinterpret_cast<temporal_rs::capi::ZonedDateTime*>(ptr));
}


#endif // temporal_rs_ZonedDateTime_HPP
