#ifndef temporal_rs_PlainMonthDay_HPP
#define temporal_rs_PlainMonthDay_HPP

#include "PlainMonthDay.d.hpp"

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
#include "PartialDate.hpp"
#include "PlainDate.hpp"
#include "TemporalError.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_PlainMonthDay_create_with_overflow_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_create_with_overflow_result;
    temporal_rs_PlainMonthDay_create_with_overflow_result temporal_rs_PlainMonthDay_create_with_overflow(uint8_t month, uint8_t day, const temporal_rs::capi::Calendar* calendar, temporal_rs::capi::ArithmeticOverflow overflow, diplomat::capi::OptionI32 ref_year);

    typedef struct temporal_rs_PlainMonthDay_with_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_with_result;
    temporal_rs_PlainMonthDay_with_result temporal_rs_PlainMonthDay_with(const temporal_rs::capi::PlainMonthDay* self, temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow overflow);

    bool temporal_rs_PlainMonthDay_equals(const temporal_rs::capi::PlainMonthDay* self, const temporal_rs::capi::PlainMonthDay* other);

    int8_t temporal_rs_PlainMonthDay_compare(const temporal_rs::capi::PlainMonthDay* one, const temporal_rs::capi::PlainMonthDay* two);

    typedef struct temporal_rs_PlainMonthDay_from_utf8_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_utf8_result;
    temporal_rs_PlainMonthDay_from_utf8_result temporal_rs_PlainMonthDay_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_PlainMonthDay_from_utf16_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_utf16_result;
    temporal_rs_PlainMonthDay_from_utf16_result temporal_rs_PlainMonthDay_from_utf16(diplomat::capi::DiplomatString16View s);

    int32_t temporal_rs_PlainMonthDay_iso_year(const temporal_rs::capi::PlainMonthDay* self);

    uint8_t temporal_rs_PlainMonthDay_iso_month(const temporal_rs::capi::PlainMonthDay* self);

    uint8_t temporal_rs_PlainMonthDay_iso_day(const temporal_rs::capi::PlainMonthDay* self);

    const temporal_rs::capi::Calendar* temporal_rs_PlainMonthDay_calendar(const temporal_rs::capi::PlainMonthDay* self);

    void temporal_rs_PlainMonthDay_month_code(const temporal_rs::capi::PlainMonthDay* self, diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_PlainMonthDay_to_plain_date_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_to_plain_date_result;
    temporal_rs_PlainMonthDay_to_plain_date_result temporal_rs_PlainMonthDay_to_plain_date(const temporal_rs::capi::PlainMonthDay* self);

    void temporal_rs_PlainMonthDay_destroy(PlainMonthDay* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::create_with_overflow(uint8_t month, uint8_t day, const temporal_rs::Calendar& calendar, temporal_rs::ArithmeticOverflow overflow, std::optional<int32_t> ref_year) {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_create_with_overflow(month,
    day,
    calendar.AsFFI(),
    overflow.AsFFI(),
    ref_year.has_value() ? (diplomat::capi::OptionI32{ { ref_year.value() }, true }) : (diplomat::capi::OptionI32{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::with(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_with(this->AsFFI(),
    partial.AsFFI(),
    overflow.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::PlainMonthDay::equals(const temporal_rs::PlainMonthDay& other) const {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_equals(this->AsFFI(),
    other.AsFFI());
  return result;
}

inline int8_t temporal_rs::PlainMonthDay::compare(const temporal_rs::PlainMonthDay& one, const temporal_rs::PlainMonthDay& two) {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_compare(one.AsFFI(),
    two.AsFFI());
  return result;
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline int32_t temporal_rs::PlainMonthDay::iso_year() const {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_iso_year(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::PlainMonthDay::iso_month() const {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_iso_month(this->AsFFI());
  return result;
}

inline uint8_t temporal_rs::PlainMonthDay::iso_day() const {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_iso_day(this->AsFFI());
  return result;
}

inline const temporal_rs::Calendar& temporal_rs::PlainMonthDay::calendar() const {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_calendar(this->AsFFI());
  return *temporal_rs::Calendar::FromFFI(result);
}

inline std::string temporal_rs::PlainMonthDay::month_code() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  temporal_rs::capi::temporal_rs_PlainMonthDay_month_code(this->AsFFI(),
    &write);
  return output;
}

inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::to_plain_date() const {
  auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_to_plain_date(this->AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::PlainMonthDay* temporal_rs::PlainMonthDay::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::PlainMonthDay*>(this);
}

inline temporal_rs::capi::PlainMonthDay* temporal_rs::PlainMonthDay::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::PlainMonthDay*>(this);
}

inline const temporal_rs::PlainMonthDay* temporal_rs::PlainMonthDay::FromFFI(const temporal_rs::capi::PlainMonthDay* ptr) {
  return reinterpret_cast<const temporal_rs::PlainMonthDay*>(ptr);
}

inline temporal_rs::PlainMonthDay* temporal_rs::PlainMonthDay::FromFFI(temporal_rs::capi::PlainMonthDay* ptr) {
  return reinterpret_cast<temporal_rs::PlainMonthDay*>(ptr);
}

inline void temporal_rs::PlainMonthDay::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_PlainMonthDay_destroy(reinterpret_cast<temporal_rs::capi::PlainMonthDay*>(ptr));
}


#endif // temporal_rs_PlainMonthDay_HPP
