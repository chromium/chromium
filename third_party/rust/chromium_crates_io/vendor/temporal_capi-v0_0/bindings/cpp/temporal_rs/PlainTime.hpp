#ifndef temporal_rs_PlainTime_HPP
#define temporal_rs_PlainTime_HPP

#include "PlainTime.d.hpp"

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
#include "DifferenceSettings.hpp"
#include "Duration.hpp"
#include "PartialTime.hpp"
#include "RoundingMode.hpp"
#include "TemporalError.hpp"
#include "TimeDuration.hpp"
#include "ToStringRoundingOptions.hpp"
#include "Unit.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_PlainTime_create_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_create_result;
    temporal_rs_PlainTime_create_result temporal_rs_PlainTime_create(uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond);

    typedef struct temporal_rs_PlainTime_try_create_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_try_create_result;
    temporal_rs_PlainTime_try_create_result temporal_rs_PlainTime_try_create(uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond);

    typedef struct temporal_rs_PlainTime_from_partial_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_from_partial_result;
    temporal_rs_PlainTime_from_partial_result temporal_rs_PlainTime_from_partial(temporal_rs::capi::PartialTime partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainTime_with_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_with_result;
    temporal_rs_PlainTime_with_result temporal_rs_PlainTime_with(const temporal_rs::capi::PlainTime* self, temporal_rs::capi::PartialTime partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainTime_from_utf8_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_from_utf8_result;
    temporal_rs_PlainTime_from_utf8_result temporal_rs_PlainTime_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_PlainTime_from_utf16_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_from_utf16_result;
    temporal_rs_PlainTime_from_utf16_result temporal_rs_PlainTime_from_utf16(diplomat::capi::DiplomatString16View s);

    uint8_t temporal_rs_PlainTime_hour(const temporal_rs::capi::PlainTime* self);

    uint8_t temporal_rs_PlainTime_minute(const temporal_rs::capi::PlainTime* self);

    uint8_t temporal_rs_PlainTime_second(const temporal_rs::capi::PlainTime* self);

    uint16_t temporal_rs_PlainTime_millisecond(const temporal_rs::capi::PlainTime* self);

    uint16_t temporal_rs_PlainTime_microsecond(const temporal_rs::capi::PlainTime* self);

    uint16_t temporal_rs_PlainTime_nanosecond(const temporal_rs::capi::PlainTime* self);

    typedef struct temporal_rs_PlainTime_add_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_add_result;
    temporal_rs_PlainTime_add_result temporal_rs_PlainTime_add(const temporal_rs::capi::PlainTime* self, const temporal_rs::capi::Duration* duration);

    typedef struct temporal_rs_PlainTime_subtract_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_subtract_result;
    temporal_rs_PlainTime_subtract_result temporal_rs_PlainTime_subtract(const temporal_rs::capi::PlainTime* self, const temporal_rs::capi::Duration* duration);

    typedef struct temporal_rs_PlainTime_add_time_duration_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_add_time_duration_result;
    temporal_rs_PlainTime_add_time_duration_result temporal_rs_PlainTime_add_time_duration(const temporal_rs::capi::PlainTime* self, const temporal_rs::capi::TimeDuration* duration);

    typedef struct temporal_rs_PlainTime_subtract_time_duration_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_subtract_time_duration_result;
    temporal_rs_PlainTime_subtract_time_duration_result temporal_rs_PlainTime_subtract_time_duration(const temporal_rs::capi::PlainTime* self, const temporal_rs::capi::TimeDuration* duration);

    typedef struct temporal_rs_PlainTime_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_until_result;
    temporal_rs_PlainTime_until_result temporal_rs_PlainTime_until(const temporal_rs::capi::PlainTime* self, const temporal_rs::capi::PlainTime* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_PlainTime_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_since_result;
    temporal_rs_PlainTime_since_result temporal_rs_PlainTime_since(const temporal_rs::capi::PlainTime* self, const temporal_rs::capi::PlainTime* other, temporal_rs::capi::DifferenceSettings settings);

    bool temporal_rs_PlainTime_equals(const temporal_rs::capi::PlainTime* self, const temporal_rs::capi::PlainTime* other);

    int8_t temporal_rs_PlainTime_compare(const temporal_rs::capi::PlainTime* one, const temporal_rs::capi::PlainTime* two);

    typedef struct temporal_rs_PlainTime_round_result {union {temporal_rs::capi::PlainTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_round_result;
    temporal_rs_PlainTime_round_result temporal_rs_PlainTime_round(const temporal_rs::capi::PlainTime* self, temporal_rs::capi::Unit smallest_unit, diplomat::capi::OptionF64 rounding_increment, temporal_rs::capi::RoundingMode_option rounding_mode);

    typedef struct temporal_rs_PlainTime_to_ixdtf_string_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainTime_to_ixdtf_string_result;
    temporal_rs_PlainTime_to_ixdtf_string_result temporal_rs_PlainTime_to_ixdtf_string(const temporal_rs::capi::PlainTime* self, temporal_rs::capi::ToStringRoundingOptions options, diplomat::capi::DiplomatWrite* write);

    void temporal_rs_PlainTime_destroy(PlainTime* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::create(uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond) {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_create(hour,
    minute,
    second,
    millisecond,
    microsecond,
    nanosecond);
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::try_create(uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond) {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_try_create(hour,
    minute,
    second,
    millisecond,
    microsecond,
    nanosecond);
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::from_partial(temporal_rs::PartialTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_from_partial(partial.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::with(temporal_rs::PartialTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_with(this->AsFFI(),
    partial.AsFFI(),
    overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint8_t temporal_rs::PlainTime::hour() const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_hour(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::PlainTime::minute() const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_minute(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::PlainTime::second() const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_second(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::PlainTime::millisecond() const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_millisecond(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::PlainTime::microsecond() const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_microsecond(this->AsFFI());
  return result;
}

inline uint16_t temporal_rs::PlainTime::nanosecond() const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_nanosecond(this->AsFFI());
  return result;
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::add(const temporal_rs::Duration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_add(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::subtract(const temporal_rs::Duration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_subtract(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::add_time_duration(const temporal_rs::TimeDuration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_add_time_duration(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::subtract_time_duration(const temporal_rs::TimeDuration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_subtract_time_duration(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainTime::until(const temporal_rs::PlainTime& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_until(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainTime::since(const temporal_rs::PlainTime& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_since(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::PlainTime::equals(const temporal_rs::PlainTime& other) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_equals(this->AsFFI(),
    other.AsFFI());
  return result;
}

inline int8_t temporal_rs::PlainTime::compare(const temporal_rs::PlainTime& one, const temporal_rs::PlainTime& two) {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_compare(one.AsFFI(),
    two.AsFFI());
  return result;
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> temporal_rs::PlainTime::round(temporal_rs::Unit smallest_unit, std::optional<double> rounding_increment, std::optional<temporal_rs::RoundingMode> rounding_mode) const {
  auto result = temporal_rs::capi::temporal_rs_PlainTime_round(this->AsFFI(),
    smallest_unit.AsFFI(),
    rounding_increment.has_value() ? (diplomat::capi::OptionF64{ { rounding_increment.value() }, true }) : (diplomat::capi::OptionF64{ {}, false }),
    rounding_mode.has_value() ? (temporal_rs::capi::RoundingMode_option{ { rounding_mode.value().AsFFI() }, true }) : (temporal_rs::capi::RoundingMode_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainTime>>(std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::PlainTime::to_ixdtf_string(temporal_rs::ToStringRoundingOptions options) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = temporal_rs::capi::temporal_rs_PlainTime_to_ixdtf_string(this->AsFFI(),
    options.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::PlainTime* temporal_rs::PlainTime::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::PlainTime*>(this);
}

inline temporal_rs::capi::PlainTime* temporal_rs::PlainTime::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::PlainTime*>(this);
}

inline const temporal_rs::PlainTime* temporal_rs::PlainTime::FromFFI(const temporal_rs::capi::PlainTime* ptr) {
  return reinterpret_cast<const temporal_rs::PlainTime*>(ptr);
}

inline temporal_rs::PlainTime* temporal_rs::PlainTime::FromFFI(temporal_rs::capi::PlainTime* ptr) {
  return reinterpret_cast<temporal_rs::PlainTime*>(ptr);
}

inline void temporal_rs::PlainTime::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_PlainTime_destroy(reinterpret_cast<temporal_rs::capi::PlainTime*>(ptr));
}


#endif // temporal_rs_PlainTime_HPP
