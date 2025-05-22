#ifndef temporal_rs_PlainDate_HPP
#define temporal_rs_PlainDate_HPP

#include "PlainDate.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "ArithmeticOverflow.hpp"
#include "Calendar.hpp"
#include "DifferenceSettings.hpp"
#include "DisplayCalendar.hpp"
#include "Duration.hpp"
#include "PartialDate.hpp"
#include "PlainDateTime.hpp"
#include "PlainMonthDay.hpp"
#include "PlainTime.hpp"
#include "PlainYearMonth.hpp"
#include "TemporalError.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_PlainDate_create_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_create_result;
    temporal_rs_PlainDate_create_result temporal_rs_PlainDate_create(int32_t year, uint8_t month, uint8_t day, const temporal_rs::capi::Calendar* calendar);

    typedef struct temporal_rs_PlainDate_try_create_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_try_create_result;
    temporal_rs_PlainDate_try_create_result temporal_rs_PlainDate_try_create(int32_t year, uint8_t month, uint8_t day, const temporal_rs::capi::Calendar* calendar);

    typedef struct temporal_rs_PlainDate_create_with_overflow_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_create_with_overflow_result;
    temporal_rs_PlainDate_create_with_overflow_result temporal_rs_PlainDate_create_with_overflow(int32_t year, uint8_t month, uint8_t day, const temporal_rs::capi::Calendar* calendar, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_PlainDate_from_partial_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_partial_result;
    temporal_rs_PlainDate_from_partial_result temporal_rs_PlainDate_from_partial(temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDate_with_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_with_result;
    temporal_rs_PlainDate_with_result temporal_rs_PlainDate_with(const temporal_rs::capi::PlainDate* self, temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDate_with_calendar_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_with_calendar_result;
    temporal_rs_PlainDate_with_calendar_result temporal_rs_PlainDate_with_calendar(const temporal_rs::capi::PlainDate* self, const temporal_rs::capi::Calendar* calendar);

    typedef struct temporal_rs_PlainDate_from_utf8_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_utf8_result;
    temporal_rs_PlainDate_from_utf8_result temporal_rs_PlainDate_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_PlainDate_from_utf16_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_utf16_result;
    temporal_rs_PlainDate_from_utf16_result temporal_rs_PlainDate_from_utf16(diplomat::capi::DiplomatString16View s);

    int32_t temporal_rs_PlainDate_iso_year(const temporal_rs::capi::PlainDate* self);

    uint8_t temporal_rs_PlainDate_iso_month(const temporal_rs::capi::PlainDate* self);

    uint8_t temporal_rs_PlainDate_iso_day(const temporal_rs::capi::PlainDate* self);

    const temporal_rs::capi::Calendar* temporal_rs_PlainDate_calendar(const temporal_rs::capi::PlainDate* self);

    bool temporal_rs_PlainDate_is_valid(const temporal_rs::capi::PlainDate* self);

    typedef struct temporal_rs_PlainDate_add_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_add_result;
    temporal_rs_PlainDate_add_result temporal_rs_PlainDate_add(const temporal_rs::capi::PlainDate* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDate_subtract_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_subtract_result;
    temporal_rs_PlainDate_subtract_result temporal_rs_PlainDate_subtract(const temporal_rs::capi::PlainDate* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDate_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_until_result;
    temporal_rs_PlainDate_until_result temporal_rs_PlainDate_until(const temporal_rs::capi::PlainDate* self, const temporal_rs::capi::PlainDate* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_PlainDate_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_since_result;
    temporal_rs_PlainDate_since_result temporal_rs_PlainDate_since(const temporal_rs::capi::PlainDate* self, const temporal_rs::capi::PlainDate* other, temporal_rs::capi::DifferenceSettings settings);

    bool temporal_rs_PlainDate_equals(const temporal_rs::capi::PlainDate* self, const temporal_rs::capi::PlainDate* other);

    int8_t temporal_rs_PlainDate_compare(const temporal_rs::capi::PlainDate* one, const temporal_rs::capi::PlainDate* two);

    int32_t temporal_rs_PlainDate_year(const temporal_rs::capi::PlainDate* self);

    uint8_t temporal_rs_PlainDate_month(const temporal_rs::capi::PlainDate* self);

    void temporal_rs_PlainDate_month_code(const temporal_rs::capi::PlainDate* self, diplomat::capi::DiplomatWrite* write);

    uint8_t temporal_rs_PlainDate_day(const temporal_rs::capi::PlainDate* self);

    typedef struct temporal_rs_PlainDate_day_of_week_result {union {uint16_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_day_of_week_result;
    temporal_rs_PlainDate_day_of_week_result temporal_rs_PlainDate_day_of_week(const temporal_rs::capi::PlainDate* self);

    uint16_t temporal_rs_PlainDate_day_of_year(const temporal_rs::capi::PlainDate* self);

    typedef struct temporal_rs_PlainDate_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_PlainDate_week_of_year_result;
    temporal_rs_PlainDate_week_of_year_result temporal_rs_PlainDate_week_of_year(const temporal_rs::capi::PlainDate* self);

    typedef struct temporal_rs_PlainDate_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDate_year_of_week_result;
    temporal_rs_PlainDate_year_of_week_result temporal_rs_PlainDate_year_of_week(const temporal_rs::capi::PlainDate* self);

    typedef struct temporal_rs_PlainDate_days_in_week_result {union {uint16_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_days_in_week_result;
    temporal_rs_PlainDate_days_in_week_result temporal_rs_PlainDate_days_in_week(const temporal_rs::capi::PlainDate* self);

    uint16_t temporal_rs_PlainDate_days_in_month(const temporal_rs::capi::PlainDate* self);

    uint16_t temporal_rs_PlainDate_days_in_year(const temporal_rs::capi::PlainDate* self);

    uint16_t temporal_rs_PlainDate_months_in_year(const temporal_rs::capi::PlainDate* self);

    bool temporal_rs_PlainDate_in_leap_year(const temporal_rs::capi::PlainDate* self);

    void temporal_rs_PlainDate_era(const temporal_rs::capi::PlainDate* self, diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_PlainDate_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDate_era_year_result;
    temporal_rs_PlainDate_era_year_result temporal_rs_PlainDate_era_year(const temporal_rs::capi::PlainDate* self);

    typedef struct temporal_rs_PlainDate_to_plain_date_time_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_plain_date_time_result;
    temporal_rs_PlainDate_to_plain_date_time_result temporal_rs_PlainDate_to_plain_date_time(const temporal_rs::capi::PlainDate* self, const temporal_rs::capi::PlainTime* time);

    typedef struct temporal_rs_PlainDate_to_plain_month_day_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_plain_month_day_result;
    temporal_rs_PlainDate_to_plain_month_day_result temporal_rs_PlainDate_to_plain_month_day(const temporal_rs::capi::PlainDate* self);

    typedef struct temporal_rs_PlainDate_to_plain_year_month_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_plain_year_month_result;
    temporal_rs_PlainDate_to_plain_year_month_result temporal_rs_PlainDate_to_plain_year_month(const temporal_rs::capi::PlainDate* self);

    void temporal_rs_PlainDate_to_ixdtf_string(const temporal_rs::capi::PlainDate* self, temporal_rs::capi::DisplayCalendar display_calendar, diplomat::capi::DiplomatWrite* write);

    void temporal_rs_PlainDate_destroy(PlainDate* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::create(int32_t year, uint8_t month, uint8_t day, const temporal_rs::Calendar& calendar) {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_create(year,
    month,
    day,
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::try_create(int32_t year, uint8_t month, uint8_t day, const temporal_rs::Calendar& calendar) {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_try_create(year,
    month,
    day,
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::create_with_overflow(int32_t year, uint8_t month, uint8_t day, const temporal_rs::Calendar& calendar, temporal_rs::ArithmeticOverflow overflow) {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_create_with_overflow(year,
    month,
    day,
    calendar.AsFFI(),
    overflow.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::from_partial(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_from_partial(partial.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::with(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_with(this->AsFFI(),
    partial.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::with_calendar(const temporal_rs::Calendar& calendar) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_with_calendar(this->AsFFI(),
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline int32_t temporal_rs::PlainDate::iso_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_iso_year(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::PlainDate::iso_month() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_iso_month(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::PlainDate::iso_day() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_iso_day(this->AsFFI());
  return result;
}

inline const temporal_rs::Calendar& temporal_rs::PlainDate::calendar() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_calendar(this->AsFFI());
  return *temporal_rs::Calendar::FromFFI(result);
}

inline bool temporal_rs::PlainDate::is_valid() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_is_valid(this->AsFFI());
  return result;
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::add(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_add(this->AsFFI(),
    duration.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainDate::subtract(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_subtract(this->AsFFI(),
    duration.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainDate::until(const temporal_rs::PlainDate& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_until(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainDate::since(const temporal_rs::PlainDate& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_since(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::PlainDate::equals(const temporal_rs::PlainDate& other) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_equals(this->AsFFI(),
    other.AsFFI());
  return result;
}

inline int8_t temporal_rs::PlainDate::compare(const temporal_rs::PlainDate& one, const temporal_rs::PlainDate& two) {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_compare(one.AsFFI(),
    two.AsFFI());
  return result;
}

inline int32_t temporal_rs::PlainDate::year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_year(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::PlainDate::month() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_month(this->AsFFI());
  return result;
}

inline std::string temporal_rs::PlainDate::month_code() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  temporal_rs::capi::temporal_rs_PlainDate_month_code(this->AsFFI(),
    &write);
  return output;
}

inline uint8_t temporal_rs::PlainDate::day() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_day(this->AsFFI());
  return result;
}

inline diplomat::result<uint16_t, temporal_rs::TemporalError> temporal_rs::PlainDate::day_of_week() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_day_of_week(this->AsFFI());
  return result.is_ok ? diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Ok<uint16_t>(result.ok)) : diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint16_t temporal_rs::PlainDate::day_of_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_day_of_year(this->AsFFI());
  return result;
}

inline std::optional<uint8_t> temporal_rs::PlainDate::week_of_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_week_of_year(this->AsFFI());
  return result.is_ok ? std::optional<uint8_t>(result.ok) : std::nullopt;
}

inline std::optional<int32_t> temporal_rs::PlainDate::year_of_week() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_year_of_week(this->AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline diplomat::result<uint16_t, temporal_rs::TemporalError> temporal_rs::PlainDate::days_in_week() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_days_in_week(this->AsFFI());
  return result.is_ok ? diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Ok<uint16_t>(result.ok)) : diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint16_t temporal_rs::PlainDate::days_in_month() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_days_in_month(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::PlainDate::days_in_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_days_in_year(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::PlainDate::months_in_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_months_in_year(this->AsFFI());
  return result;
}

inline bool temporal_rs::PlainDate::in_leap_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_in_leap_year(this->AsFFI());
  return result;
}

inline std::string temporal_rs::PlainDate::era() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  temporal_rs::capi::temporal_rs_PlainDate_era(this->AsFFI(),
    &write);
  return output;
}

inline std::optional<int32_t> temporal_rs::PlainDate::era_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_era_year(this->AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDate::to_plain_date_time(const temporal_rs::PlainTime* time) const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_to_plain_date_time(this->AsFFI(),
    time ? time->AsFFI() : nullptr);
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainDate::to_plain_month_day() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_to_plain_month_day(this->AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainDate::to_plain_year_month() const {
  auto result = temporal_rs::capi::temporal_rs_PlainDate_to_plain_year_month(this->AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::string temporal_rs::PlainDate::to_ixdtf_string(temporal_rs::DisplayCalendar display_calendar) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  temporal_rs::capi::temporal_rs_PlainDate_to_ixdtf_string(this->AsFFI(),
    display_calendar.AsFFI(),
    &write);
  return output;
}

inline const temporal_rs::capi::PlainDate* temporal_rs::PlainDate::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::PlainDate*>(this);
}

inline temporal_rs::capi::PlainDate* temporal_rs::PlainDate::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::PlainDate*>(this);
}

inline const temporal_rs::PlainDate* temporal_rs::PlainDate::FromFFI(const temporal_rs::capi::PlainDate* ptr) {
  return reinterpret_cast<const temporal_rs::PlainDate*>(ptr);
}

inline temporal_rs::PlainDate* temporal_rs::PlainDate::FromFFI(temporal_rs::capi::PlainDate* ptr) {
  return reinterpret_cast<temporal_rs::PlainDate*>(ptr);
}

inline void temporal_rs::PlainDate::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_PlainDate_destroy(reinterpret_cast<temporal_rs::capi::PlainDate*>(ptr));
}


#endif // temporal_rs_PlainDate_HPP
