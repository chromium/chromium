#ifndef temporal_rs_Instant_HPP
#define temporal_rs_Instant_HPP

#include "Instant.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "DifferenceSettings.hpp"
#include "Duration.hpp"
#include "I128Nanoseconds.hpp"
#include "RoundingOptions.hpp"
#include "TemporalError.hpp"
#include "TimeDuration.hpp"
#include "TimeZone.hpp"
#include "ToStringRoundingOptions.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_Instant_try_new_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_try_new_result;
    temporal_rs_Instant_try_new_result temporal_rs_Instant_try_new(temporal_rs::capi::I128Nanoseconds ns);

    typedef struct temporal_rs_Instant_from_epoch_milliseconds_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_epoch_milliseconds_result;
    temporal_rs_Instant_from_epoch_milliseconds_result temporal_rs_Instant_from_epoch_milliseconds(int64_t epoch_milliseconds);

    typedef struct temporal_rs_Instant_from_utf8_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_utf8_result;
    temporal_rs_Instant_from_utf8_result temporal_rs_Instant_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_Instant_from_utf16_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_utf16_result;
    temporal_rs_Instant_from_utf16_result temporal_rs_Instant_from_utf16(diplomat::capi::DiplomatString16View s);

    typedef struct temporal_rs_Instant_add_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_add_result;
    temporal_rs_Instant_add_result temporal_rs_Instant_add(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Duration* duration);

    typedef struct temporal_rs_Instant_add_time_duration_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_add_time_duration_result;
    temporal_rs_Instant_add_time_duration_result temporal_rs_Instant_add_time_duration(const temporal_rs::capi::Instant* self, const temporal_rs::capi::TimeDuration* duration);

    typedef struct temporal_rs_Instant_subtract_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_subtract_result;
    temporal_rs_Instant_subtract_result temporal_rs_Instant_subtract(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Duration* duration);

    typedef struct temporal_rs_Instant_subtract_time_duration_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_subtract_time_duration_result;
    temporal_rs_Instant_subtract_time_duration_result temporal_rs_Instant_subtract_time_duration(const temporal_rs::capi::Instant* self, const temporal_rs::capi::TimeDuration* duration);

    typedef struct temporal_rs_Instant_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_since_result;
    temporal_rs_Instant_since_result temporal_rs_Instant_since(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Instant* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_Instant_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_until_result;
    temporal_rs_Instant_until_result temporal_rs_Instant_until(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Instant* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_Instant_round_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_round_result;
    temporal_rs_Instant_round_result temporal_rs_Instant_round(const temporal_rs::capi::Instant* self, temporal_rs::capi::RoundingOptions options);

    int64_t temporal_rs_Instant_epoch_milliseconds(const temporal_rs::capi::Instant* self);

    temporal_rs::capi::I128Nanoseconds temporal_rs_Instant_epoch_nanoseconds(const temporal_rs::capi::Instant* self);

    typedef struct temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result;
    temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result temporal_rs_Instant_to_ixdtf_string_with_compiled_data(const temporal_rs::capi::Instant* self, const temporal_rs::capi::TimeZone* zone, temporal_rs::capi::ToStringRoundingOptions options, diplomat::capi::DiplomatWrite* write);

    void temporal_rs_Instant_destroy(Instant* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::try_new(temporal_rs::I128Nanoseconds ns) {
  auto result = temporal_rs::capi::temporal_rs_Instant_try_new(ns.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::from_epoch_milliseconds(int64_t epoch_milliseconds) {
  auto result = temporal_rs::capi::temporal_rs_Instant_from_epoch_milliseconds(epoch_milliseconds);
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_Instant_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_Instant_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::add(const temporal_rs::Duration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_Instant_add(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::add_time_duration(const temporal_rs::TimeDuration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_Instant_add_time_duration(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::subtract(const temporal_rs::Duration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_Instant_subtract(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::subtract_time_duration(const temporal_rs::TimeDuration& duration) const {
  auto result = temporal_rs::capi::temporal_rs_Instant_subtract_time_duration(this->AsFFI(),
    duration.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Instant::since(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_Instant_since(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Instant::until(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const {
  auto result = temporal_rs::capi::temporal_rs_Instant_until(this->AsFFI(),
    other.AsFFI(),
    settings.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::round(temporal_rs::RoundingOptions options) const {
  auto result = temporal_rs::capi::temporal_rs_Instant_round(this->AsFFI(),
    options.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline int64_t temporal_rs::Instant::epoch_milliseconds() const {
  auto result = temporal_rs::capi::temporal_rs_Instant_epoch_milliseconds(this->AsFFI());
  return result;
}

inline temporal_rs::I128Nanoseconds temporal_rs::Instant::epoch_nanoseconds() const {
  auto result = temporal_rs::capi::temporal_rs_Instant_epoch_nanoseconds(this->AsFFI());
  return temporal_rs::I128Nanoseconds::FromFFI(result);
}

inline diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::Instant::to_ixdtf_string_with_compiled_data(const temporal_rs::TimeZone* zone, temporal_rs::ToStringRoundingOptions options) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = temporal_rs::capi::temporal_rs_Instant_to_ixdtf_string_with_compiled_data(this->AsFFI(),
    zone ? zone->AsFFI() : nullptr,
    options.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::Instant* temporal_rs::Instant::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::Instant*>(this);
}

inline temporal_rs::capi::Instant* temporal_rs::Instant::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::Instant*>(this);
}

inline const temporal_rs::Instant* temporal_rs::Instant::FromFFI(const temporal_rs::capi::Instant* ptr) {
  return reinterpret_cast<const temporal_rs::Instant*>(ptr);
}

inline temporal_rs::Instant* temporal_rs::Instant::FromFFI(temporal_rs::capi::Instant* ptr) {
  return reinterpret_cast<temporal_rs::Instant*>(ptr);
}

inline void temporal_rs::Instant::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_Instant_destroy(reinterpret_cast<temporal_rs::capi::Instant*>(ptr));
}


#endif // temporal_rs_Instant_HPP
