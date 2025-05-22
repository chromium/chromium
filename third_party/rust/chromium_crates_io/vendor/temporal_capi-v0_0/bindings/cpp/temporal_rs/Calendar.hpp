#ifndef temporal_rs_Calendar_HPP
#define temporal_rs_Calendar_HPP

#include "Calendar.d.hpp"

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
#include "Duration.hpp"
#include "IsoDate.hpp"
#include "PartialDate.hpp"
#include "PlainDate.hpp"
#include "PlainMonthDay.hpp"
#include "PlainYearMonth.hpp"
#include "TemporalError.hpp"
#include "Unit.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    temporal_rs::capi::Calendar* temporal_rs_Calendar_create(temporal_rs::capi::AnyCalendarKind kind);

    typedef struct temporal_rs_Calendar_from_utf8_result {union {temporal_rs::capi::Calendar* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_from_utf8_result;
    temporal_rs_Calendar_from_utf8_result temporal_rs_Calendar_from_utf8(diplomat::capi::DiplomatStringView s);

    bool temporal_rs_Calendar_is_iso(const temporal_rs::capi::Calendar* self);

    diplomat::capi::DiplomatStringView temporal_rs_Calendar_identifier(const temporal_rs::capi::Calendar* self);

    typedef struct temporal_rs_Calendar_date_from_partial_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_date_from_partial_result;
    temporal_rs_Calendar_date_from_partial_result temporal_rs_Calendar_date_from_partial(const temporal_rs::capi::Calendar* self, temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_Calendar_month_day_from_partial_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_month_day_from_partial_result;
    temporal_rs_Calendar_month_day_from_partial_result temporal_rs_Calendar_month_day_from_partial(const temporal_rs::capi::Calendar* self, temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_Calendar_year_month_from_partial_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_year_month_from_partial_result;
    temporal_rs_Calendar_year_month_from_partial_result temporal_rs_Calendar_year_month_from_partial(const temporal_rs::capi::Calendar* self, temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_Calendar_date_add_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_date_add_result;
    temporal_rs_Calendar_date_add_result temporal_rs_Calendar_date_add(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_Calendar_date_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_date_until_result;
    temporal_rs_Calendar_date_until_result temporal_rs_Calendar_date_until(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate one, temporal_rs::capi::IsoDate two, temporal_rs::capi::Unit largest_unit);

    typedef struct temporal_rs_Calendar_era_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_era_result;
    temporal_rs_Calendar_era_result temporal_rs_Calendar_era(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date, diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_Calendar_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_Calendar_era_year_result;
    temporal_rs_Calendar_era_year_result temporal_rs_Calendar_era_year(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    int32_t temporal_rs_Calendar_year(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    uint8_t temporal_rs_Calendar_month(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    typedef struct temporal_rs_Calendar_month_code_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_month_code_result;
    temporal_rs_Calendar_month_code_result temporal_rs_Calendar_month_code(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date, diplomat::capi::DiplomatWrite* write);

    uint8_t temporal_rs_Calendar_day(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    typedef struct temporal_rs_Calendar_day_of_week_result {union {uint16_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_day_of_week_result;
    temporal_rs_Calendar_day_of_week_result temporal_rs_Calendar_day_of_week(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    uint16_t temporal_rs_Calendar_day_of_year(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    typedef struct temporal_rs_Calendar_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_Calendar_week_of_year_result;
    temporal_rs_Calendar_week_of_year_result temporal_rs_Calendar_week_of_year(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    typedef struct temporal_rs_Calendar_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_Calendar_year_of_week_result;
    temporal_rs_Calendar_year_of_week_result temporal_rs_Calendar_year_of_week(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    typedef struct temporal_rs_Calendar_days_in_week_result {union {uint16_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_days_in_week_result;
    temporal_rs_Calendar_days_in_week_result temporal_rs_Calendar_days_in_week(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    uint16_t temporal_rs_Calendar_days_in_month(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    uint16_t temporal_rs_Calendar_days_in_year(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    uint16_t temporal_rs_Calendar_months_in_year(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    bool temporal_rs_Calendar_in_leap_year(const temporal_rs::capi::Calendar* self, temporal_rs::capi::IsoDate date);

    void temporal_rs_Calendar_destroy(Calendar* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<temporal_rs::Calendar> temporal_rs::Calendar::create(temporal_rs::AnyCalendarKind kind) {
  auto result = temporal_rs::capi::temporal_rs_Calendar_create(kind.AsFFI());
  return std::unique_ptr<temporal_rs::Calendar>(temporal_rs::Calendar::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError> temporal_rs::Calendar::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_Calendar_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Calendar>>(std::unique_ptr<temporal_rs::Calendar>(temporal_rs::Calendar::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::Calendar::is_iso() const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_is_iso(this->AsFFI());
  return result;
}

inline std::string_view temporal_rs::Calendar::identifier() const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_identifier(this->AsFFI());
  return std::string_view(result.data, result.len);
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::Calendar::date_from_partial(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_date_from_partial(this->AsFFI(),
    partial.AsFFI(),
    overflow.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::Calendar::month_day_from_partial(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_month_day_from_partial(this->AsFFI(),
    partial.AsFFI(),
    overflow.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::Calendar::year_month_from_partial(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_year_month_from_partial(this->AsFFI(),
    partial.AsFFI(),
    overflow.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::Calendar::date_add(temporal_rs::IsoDate date, const temporal_rs::Duration& duration, temporal_rs::ArithmeticOverflow overflow) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_date_add(this->AsFFI(),
    date.AsFFI(),
    duration.AsFFI(),
    overflow.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Calendar::date_until(temporal_rs::IsoDate one, temporal_rs::IsoDate two, temporal_rs::Unit largest_unit) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_date_until(this->AsFFI(),
    one.AsFFI(),
    two.AsFFI(),
    largest_unit.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::Calendar::era(temporal_rs::IsoDate date) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = temporal_rs::capi::temporal_rs_Calendar_era(this->AsFFI(),
    date.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::optional<int32_t> temporal_rs::Calendar::era_year(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_era_year(this->AsFFI(),
    date.AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline int32_t temporal_rs::Calendar::year(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_year(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline uint8_t temporal_rs::Calendar::month(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_month(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::Calendar::month_code(temporal_rs::IsoDate date) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = temporal_rs::capi::temporal_rs_Calendar_month_code(this->AsFFI(),
    date.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint8_t temporal_rs::Calendar::day(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_day(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline diplomat::result<uint16_t, temporal_rs::TemporalError> temporal_rs::Calendar::day_of_week(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_day_of_week(this->AsFFI(),
    date.AsFFI());
  return result.is_ok ? diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Ok<uint16_t>(result.ok)) : diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint16_t temporal_rs::Calendar::day_of_year(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_day_of_year(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline std::optional<uint8_t> temporal_rs::Calendar::week_of_year(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_week_of_year(this->AsFFI(),
    date.AsFFI());
  return result.is_ok ? std::optional<uint8_t>(result.ok) : std::nullopt;
}

inline std::optional<int32_t> temporal_rs::Calendar::year_of_week(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_year_of_week(this->AsFFI(),
    date.AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline diplomat::result<uint16_t, temporal_rs::TemporalError> temporal_rs::Calendar::days_in_week(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_days_in_week(this->AsFFI(),
    date.AsFFI());
  return result.is_ok ? diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Ok<uint16_t>(result.ok)) : diplomat::result<uint16_t, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint16_t temporal_rs::Calendar::days_in_month(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_days_in_month(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline uint16_t temporal_rs::Calendar::days_in_year(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_days_in_year(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline uint16_t temporal_rs::Calendar::months_in_year(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_months_in_year(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline bool temporal_rs::Calendar::in_leap_year(temporal_rs::IsoDate date) const {
  auto result = temporal_rs::capi::temporal_rs_Calendar_in_leap_year(this->AsFFI(),
    date.AsFFI());
  return result;
}

inline const temporal_rs::capi::Calendar* temporal_rs::Calendar::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::Calendar*>(this);
}

inline temporal_rs::capi::Calendar* temporal_rs::Calendar::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::Calendar*>(this);
}

inline const temporal_rs::Calendar* temporal_rs::Calendar::FromFFI(const temporal_rs::capi::Calendar* ptr) {
  return reinterpret_cast<const temporal_rs::Calendar*>(ptr);
}

inline temporal_rs::Calendar* temporal_rs::Calendar::FromFFI(temporal_rs::capi::Calendar* ptr) {
  return reinterpret_cast<temporal_rs::Calendar*>(ptr);
}

inline void temporal_rs::Calendar::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_Calendar_destroy(reinterpret_cast<temporal_rs::capi::Calendar*>(ptr));
}


#endif // temporal_rs_Calendar_HPP
