#ifndef temporal_rs_Duration_HPP
#define temporal_rs_Duration_HPP

#include "Duration.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "DateDuration.hpp"
#include "PartialDuration.hpp"
#include "Sign.hpp"
#include "TemporalError.hpp"
#include "TimeDuration.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_Duration_create_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_create_result;
    temporal_rs_Duration_create_result temporal_rs_Duration_create(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

    typedef struct temporal_rs_Duration_from_day_and_time_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_day_and_time_result;
    temporal_rs_Duration_from_day_and_time_result temporal_rs_Duration_from_day_and_time(int64_t day, const temporal_rs::capi::TimeDuration* time);

    typedef struct temporal_rs_Duration_from_partial_duration_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_partial_duration_result;
    temporal_rs_Duration_from_partial_duration_result temporal_rs_Duration_from_partial_duration(temporal_rs::capi::PartialDuration partial);

    typedef struct temporal_rs_Duration_from_utf8_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_utf8_result;
    temporal_rs_Duration_from_utf8_result temporal_rs_Duration_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_Duration_from_utf16_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_utf16_result;
    temporal_rs_Duration_from_utf16_result temporal_rs_Duration_from_utf16(diplomat::capi::DiplomatString16View s);

    bool temporal_rs_Duration_is_time_within_range(const temporal_rs::capi::Duration* self);

    const temporal_rs::capi::TimeDuration* temporal_rs_Duration_time(const temporal_rs::capi::Duration* self);

    const temporal_rs::capi::DateDuration* temporal_rs_Duration_date(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_years(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_months(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_weeks(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_days(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_hours(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_minutes(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_seconds(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_milliseconds(const temporal_rs::capi::Duration* self);

    typedef struct temporal_rs_Duration_microseconds_result {union {double ok; }; bool is_ok;} temporal_rs_Duration_microseconds_result;
    temporal_rs_Duration_microseconds_result temporal_rs_Duration_microseconds(const temporal_rs::capi::Duration* self);

    typedef struct temporal_rs_Duration_nanoseconds_result {union {double ok; }; bool is_ok;} temporal_rs_Duration_nanoseconds_result;
    temporal_rs_Duration_nanoseconds_result temporal_rs_Duration_nanoseconds(const temporal_rs::capi::Duration* self);

    temporal_rs::capi::Sign temporal_rs_Duration_sign(const temporal_rs::capi::Duration* self);

    bool temporal_rs_Duration_is_zero(const temporal_rs::capi::Duration* self);

    temporal_rs::capi::Duration* temporal_rs_Duration_abs(const temporal_rs::capi::Duration* self);

    temporal_rs::capi::Duration* temporal_rs_Duration_negated(const temporal_rs::capi::Duration* self);

    typedef struct temporal_rs_Duration_add_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_add_result;
    temporal_rs_Duration_add_result temporal_rs_Duration_add(const temporal_rs::capi::Duration* self, const temporal_rs::capi::Duration* other);

    typedef struct temporal_rs_Duration_subtract_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_subtract_result;
    temporal_rs_Duration_subtract_result temporal_rs_Duration_subtract(const temporal_rs::capi::Duration* self, const temporal_rs::capi::Duration* other);

    void temporal_rs_Duration_destroy(Duration* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::create(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds) {
  auto result = temporal_rs::capi::temporal_rs_Duration_create(years,
    months,
    weeks,
    days,
    hours,
    minutes,
    seconds,
    milliseconds,
    microseconds,
    nanoseconds);
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::from_day_and_time(int64_t day, const temporal_rs::TimeDuration& time) {
  auto result = temporal_rs::capi::temporal_rs_Duration_from_day_and_time(day,
    time.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::from_partial_duration(temporal_rs::PartialDuration partial) {
  auto result = temporal_rs::capi::temporal_rs_Duration_from_partial_duration(partial.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_Duration_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_Duration_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::Duration::is_time_within_range() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_is_time_within_range(this->AsFFI());
  return result;
}

inline const temporal_rs::TimeDuration& temporal_rs::Duration::time() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_time(this->AsFFI());
  return *temporal_rs::TimeDuration::FromFFI(result);
}

inline const temporal_rs::DateDuration& temporal_rs::Duration::date() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_date(this->AsFFI());
  return *temporal_rs::DateDuration::FromFFI(result);
}

inline int64_t temporal_rs::Duration::years() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_years(this->AsFFI());
  return result;
}

inline int64_t temporal_rs::Duration::months() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_months(this->AsFFI());
  return result;
}

inline int64_t temporal_rs::Duration::weeks() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_weeks(this->AsFFI());
  return result;
}

inline int64_t temporal_rs::Duration::days() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_days(this->AsFFI());
  return result;
}

inline int64_t temporal_rs::Duration::hours() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_hours(this->AsFFI());
  return result;
}

inline int64_t temporal_rs::Duration::minutes() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_minutes(this->AsFFI());
  return result;
}

inline int64_t temporal_rs::Duration::seconds() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_seconds(this->AsFFI());
  return result;
}

inline int64_t temporal_rs::Duration::milliseconds() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_milliseconds(this->AsFFI());
  return result;
}

inline std::optional<double> temporal_rs::Duration::microseconds() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_microseconds(this->AsFFI());
  return result.is_ok ? std::optional<double>(result.ok) : std::nullopt;
}

inline std::optional<double> temporal_rs::Duration::nanoseconds() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_nanoseconds(this->AsFFI());
  return result.is_ok ? std::optional<double>(result.ok) : std::nullopt;
}

inline temporal_rs::Sign temporal_rs::Duration::sign() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_sign(this->AsFFI());
  return temporal_rs::Sign::FromFFI(result);
}

inline bool temporal_rs::Duration::is_zero() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_is_zero(this->AsFFI());
  return result;
}

inline std::unique_ptr<temporal_rs::Duration> temporal_rs::Duration::abs() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_abs(this->AsFFI());
  return std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::Duration> temporal_rs::Duration::negated() const {
  auto result = temporal_rs::capi::temporal_rs_Duration_negated(this->AsFFI());
  return std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::add(const temporal_rs::Duration& other) const {
  auto result = temporal_rs::capi::temporal_rs_Duration_add(this->AsFFI(),
    other.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::subtract(const temporal_rs::Duration& other) const {
  auto result = temporal_rs::capi::temporal_rs_Duration_subtract(this->AsFFI(),
    other.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::Duration* temporal_rs::Duration::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::Duration*>(this);
}

inline temporal_rs::capi::Duration* temporal_rs::Duration::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::Duration*>(this);
}

inline const temporal_rs::Duration* temporal_rs::Duration::FromFFI(const temporal_rs::capi::Duration* ptr) {
  return reinterpret_cast<const temporal_rs::Duration*>(ptr);
}

inline temporal_rs::Duration* temporal_rs::Duration::FromFFI(temporal_rs::capi::Duration* ptr) {
  return reinterpret_cast<temporal_rs::Duration*>(ptr);
}

inline void temporal_rs::Duration::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_Duration_destroy(reinterpret_cast<temporal_rs::capi::Duration*>(ptr));
}


#endif // temporal_rs_Duration_HPP
