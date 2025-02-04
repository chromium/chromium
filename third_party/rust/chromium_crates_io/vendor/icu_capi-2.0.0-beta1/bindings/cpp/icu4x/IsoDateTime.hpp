#ifndef icu4x_IsoDateTime_HPP
#define icu4x_IsoDateTime_HPP

#include "IsoDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "Calendar.hpp"
#include "CalendarError.hpp"
#include "CalendarParseError.hpp"
#include "DateTime.hpp"
#include "IsoDate.hpp"
#include "IsoWeekday.hpp"
#include "Time.hpp"
#include "WeekCalculator.hpp"
#include "WeekOf.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_IsoDateTime_create_mv1_result {union {icu4x::capi::IsoDateTime* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_IsoDateTime_create_mv1_result;
    icu4x_IsoDateTime_create_mv1_result icu4x_IsoDateTime_create_mv1(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond);
    
    icu4x::capi::IsoDateTime* icu4x_IsoDateTime_from_date_and_time_mv1(const icu4x::capi::IsoDate* date, const icu4x::capi::Time* time);
    
    typedef struct icu4x_IsoDateTime_from_string_mv1_result {union {icu4x::capi::IsoDateTime* ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_IsoDateTime_from_string_mv1_result;
    icu4x_IsoDateTime_from_string_mv1_result icu4x_IsoDateTime_from_string_mv1(diplomat::capi::DiplomatStringView v);
    
    icu4x::capi::IsoDate* icu4x_IsoDateTime_date_mv1(const icu4x::capi::IsoDateTime* self);
    
    icu4x::capi::Time* icu4x_IsoDateTime_time_mv1(const icu4x::capi::IsoDateTime* self);
    
    icu4x::capi::DateTime* icu4x_IsoDateTime_to_any_mv1(const icu4x::capi::IsoDateTime* self);
    
    icu4x::capi::DateTime* icu4x_IsoDateTime_to_calendar_mv1(const icu4x::capi::IsoDateTime* self, const icu4x::capi::Calendar* calendar);
    
    uint8_t icu4x_IsoDateTime_hour_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint8_t icu4x_IsoDateTime_minute_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint8_t icu4x_IsoDateTime_second_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint32_t icu4x_IsoDateTime_nanosecond_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint16_t icu4x_IsoDateTime_day_of_year_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint8_t icu4x_IsoDateTime_day_of_month_mv1(const icu4x::capi::IsoDateTime* self);
    
    icu4x::capi::IsoWeekday icu4x_IsoDateTime_day_of_week_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint8_t icu4x_IsoDateTime_week_of_month_mv1(const icu4x::capi::IsoDateTime* self, icu4x::capi::IsoWeekday first_weekday);
    
    icu4x::capi::WeekOf icu4x_IsoDateTime_week_of_year_mv1(const icu4x::capi::IsoDateTime* self, const icu4x::capi::WeekCalculator* calculator);
    
    uint8_t icu4x_IsoDateTime_month_mv1(const icu4x::capi::IsoDateTime* self);
    
    int32_t icu4x_IsoDateTime_year_mv1(const icu4x::capi::IsoDateTime* self);
    
    bool icu4x_IsoDateTime_is_in_leap_year_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint8_t icu4x_IsoDateTime_months_in_year_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint8_t icu4x_IsoDateTime_days_in_month_mv1(const icu4x::capi::IsoDateTime* self);
    
    uint16_t icu4x_IsoDateTime_days_in_year_mv1(const icu4x::capi::IsoDateTime* self);
    
    
    void icu4x_IsoDateTime_destroy_mv1(IsoDateTime* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarError> icu4x::IsoDateTime::create(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond) {
  auto result = icu4x::capi::icu4x_IsoDateTime_create_mv1(year,
    month,
    day,
    hour,
    minute,
    second,
    nanosecond);
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarError>(diplomat::Ok<std::unique_ptr<icu4x::IsoDateTime>>(std::unique_ptr<icu4x::IsoDateTime>(icu4x::IsoDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarError>(diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::IsoDateTime> icu4x::IsoDateTime::from_date_and_time(const icu4x::IsoDate& date, const icu4x::Time& time) {
  auto result = icu4x::capi::icu4x_IsoDateTime_from_date_and_time_mv1(date.AsFFI(),
    time.AsFFI());
  return std::unique_ptr<icu4x::IsoDateTime>(icu4x::IsoDateTime::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarParseError> icu4x::IsoDateTime::from_string(std::string_view v) {
  auto result = icu4x::capi::icu4x_IsoDateTime_from_string_mv1({v.data(), v.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarParseError>(diplomat::Ok<std::unique_ptr<icu4x::IsoDateTime>>(std::unique_ptr<icu4x::IsoDateTime>(icu4x::IsoDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::IsoDate> icu4x::IsoDateTime::date() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_date_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::IsoDate>(icu4x::IsoDate::FromFFI(result));
}

inline std::unique_ptr<icu4x::Time> icu4x::IsoDateTime::time() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_time_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(result));
}

inline std::unique_ptr<icu4x::DateTime> icu4x::IsoDateTime::to_any() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_to_any_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::DateTime>(icu4x::DateTime::FromFFI(result));
}

inline std::unique_ptr<icu4x::DateTime> icu4x::IsoDateTime::to_calendar(const icu4x::Calendar& calendar) const {
  auto result = icu4x::capi::icu4x_IsoDateTime_to_calendar_mv1(this->AsFFI(),
    calendar.AsFFI());
  return std::unique_ptr<icu4x::DateTime>(icu4x::DateTime::FromFFI(result));
}

inline uint8_t icu4x::IsoDateTime::hour() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_hour_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDateTime::minute() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_minute_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDateTime::second() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_second_mv1(this->AsFFI());
  return result;
}

inline uint32_t icu4x::IsoDateTime::nanosecond() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_nanosecond_mv1(this->AsFFI());
  return result;
}

inline uint16_t icu4x::IsoDateTime::day_of_year() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_day_of_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDateTime::day_of_month() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_day_of_month_mv1(this->AsFFI());
  return result;
}

inline icu4x::IsoWeekday icu4x::IsoDateTime::day_of_week() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_day_of_week_mv1(this->AsFFI());
  return icu4x::IsoWeekday::FromFFI(result);
}

inline uint8_t icu4x::IsoDateTime::week_of_month(icu4x::IsoWeekday first_weekday) const {
  auto result = icu4x::capi::icu4x_IsoDateTime_week_of_month_mv1(this->AsFFI(),
    first_weekday.AsFFI());
  return result;
}

inline icu4x::WeekOf icu4x::IsoDateTime::week_of_year(const icu4x::WeekCalculator& calculator) const {
  auto result = icu4x::capi::icu4x_IsoDateTime_week_of_year_mv1(this->AsFFI(),
    calculator.AsFFI());
  return icu4x::WeekOf::FromFFI(result);
}

inline uint8_t icu4x::IsoDateTime::month() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_month_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::IsoDateTime::year() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_year_mv1(this->AsFFI());
  return result;
}

inline bool icu4x::IsoDateTime::is_in_leap_year() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_is_in_leap_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDateTime::months_in_year() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_months_in_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDateTime::days_in_month() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_days_in_month_mv1(this->AsFFI());
  return result;
}

inline uint16_t icu4x::IsoDateTime::days_in_year() const {
  auto result = icu4x::capi::icu4x_IsoDateTime_days_in_year_mv1(this->AsFFI());
  return result;
}

inline const icu4x::capi::IsoDateTime* icu4x::IsoDateTime::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::IsoDateTime*>(this);
}

inline icu4x::capi::IsoDateTime* icu4x::IsoDateTime::AsFFI() {
  return reinterpret_cast<icu4x::capi::IsoDateTime*>(this);
}

inline const icu4x::IsoDateTime* icu4x::IsoDateTime::FromFFI(const icu4x::capi::IsoDateTime* ptr) {
  return reinterpret_cast<const icu4x::IsoDateTime*>(ptr);
}

inline icu4x::IsoDateTime* icu4x::IsoDateTime::FromFFI(icu4x::capi::IsoDateTime* ptr) {
  return reinterpret_cast<icu4x::IsoDateTime*>(ptr);
}

inline void icu4x::IsoDateTime::operator delete(void* ptr) {
  icu4x::capi::icu4x_IsoDateTime_destroy_mv1(reinterpret_cast<icu4x::capi::IsoDateTime*>(ptr));
}


#endif // icu4x_IsoDateTime_HPP
