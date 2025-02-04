#ifndef icu4x_DateTime_HPP
#define icu4x_DateTime_HPP

#include "DateTime.d.hpp"

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
#include "Date.hpp"
#include "IsoDateTime.hpp"
#include "IsoWeekday.hpp"
#include "Time.hpp"
#include "WeekCalculator.hpp"
#include "WeekOf.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_DateTime_from_iso_in_calendar_mv1_result {union {icu4x::capi::DateTime* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_DateTime_from_iso_in_calendar_mv1_result;
    icu4x_DateTime_from_iso_in_calendar_mv1_result icu4x_DateTime_from_iso_in_calendar_mv1(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const icu4x::capi::Calendar* calendar);
    
    typedef struct icu4x_DateTime_from_codes_in_calendar_mv1_result {union {icu4x::capi::DateTime* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_DateTime_from_codes_in_calendar_mv1_result;
    icu4x_DateTime_from_codes_in_calendar_mv1_result icu4x_DateTime_from_codes_in_calendar_mv1(diplomat::capi::DiplomatStringView era_code, int32_t year, diplomat::capi::DiplomatStringView month_code, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const icu4x::capi::Calendar* calendar);
    
    icu4x::capi::DateTime* icu4x_DateTime_from_date_and_time_mv1(const icu4x::capi::Date* date, const icu4x::capi::Time* time);
    
    typedef struct icu4x_DateTime_from_string_mv1_result {union {icu4x::capi::DateTime* ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_DateTime_from_string_mv1_result;
    icu4x_DateTime_from_string_mv1_result icu4x_DateTime_from_string_mv1(diplomat::capi::DiplomatStringView v);
    
    icu4x::capi::Date* icu4x_DateTime_date_mv1(const icu4x::capi::DateTime* self);
    
    icu4x::capi::Time* icu4x_DateTime_time_mv1(const icu4x::capi::DateTime* self);
    
    icu4x::capi::IsoDateTime* icu4x_DateTime_to_iso_mv1(const icu4x::capi::DateTime* self);
    
    icu4x::capi::DateTime* icu4x_DateTime_to_calendar_mv1(const icu4x::capi::DateTime* self, const icu4x::capi::Calendar* calendar);
    
    uint8_t icu4x_DateTime_hour_mv1(const icu4x::capi::DateTime* self);
    
    uint8_t icu4x_DateTime_minute_mv1(const icu4x::capi::DateTime* self);
    
    uint8_t icu4x_DateTime_second_mv1(const icu4x::capi::DateTime* self);
    
    uint32_t icu4x_DateTime_nanosecond_mv1(const icu4x::capi::DateTime* self);
    
    uint16_t icu4x_DateTime_day_of_year_mv1(const icu4x::capi::DateTime* self);
    
    uint8_t icu4x_DateTime_day_of_month_mv1(const icu4x::capi::DateTime* self);
    
    icu4x::capi::IsoWeekday icu4x_DateTime_day_of_week_mv1(const icu4x::capi::DateTime* self);
    
    uint8_t icu4x_DateTime_week_of_month_mv1(const icu4x::capi::DateTime* self, icu4x::capi::IsoWeekday first_weekday);
    
    icu4x::capi::WeekOf icu4x_DateTime_week_of_year_mv1(const icu4x::capi::DateTime* self, const icu4x::capi::WeekCalculator* calculator);
    
    uint8_t icu4x_DateTime_ordinal_month_mv1(const icu4x::capi::DateTime* self);
    
    void icu4x_DateTime_month_code_mv1(const icu4x::capi::DateTime* self, diplomat::capi::DiplomatWrite* write);
    
    uint8_t icu4x_DateTime_month_number_mv1(const icu4x::capi::DateTime* self);
    
    bool icu4x_DateTime_month_is_leap_mv1(const icu4x::capi::DateTime* self);
    
    int32_t icu4x_DateTime_year_in_era_mv1(const icu4x::capi::DateTime* self);
    
    int32_t icu4x_DateTime_extended_year_mv1(const icu4x::capi::DateTime* self);
    
    void icu4x_DateTime_era_mv1(const icu4x::capi::DateTime* self, diplomat::capi::DiplomatWrite* write);
    
    uint8_t icu4x_DateTime_months_in_year_mv1(const icu4x::capi::DateTime* self);
    
    uint8_t icu4x_DateTime_days_in_month_mv1(const icu4x::capi::DateTime* self);
    
    uint16_t icu4x_DateTime_days_in_year_mv1(const icu4x::capi::DateTime* self);
    
    icu4x::capi::Calendar* icu4x_DateTime_calendar_mv1(const icu4x::capi::DateTime* self);
    
    
    void icu4x_DateTime_destroy_mv1(DateTime* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError> icu4x::DateTime::from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const icu4x::Calendar& calendar) {
  auto result = icu4x::capi::icu4x_DateTime_from_iso_in_calendar_mv1(year,
    month,
    day,
    hour,
    minute,
    second,
    nanosecond,
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError>(diplomat::Ok<std::unique_ptr<icu4x::DateTime>>(std::unique_ptr<icu4x::DateTime>(icu4x::DateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError>(diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError> icu4x::DateTime::from_codes_in_calendar(std::string_view era_code, int32_t year, std::string_view month_code, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const icu4x::Calendar& calendar) {
  auto result = icu4x::capi::icu4x_DateTime_from_codes_in_calendar_mv1({era_code.data(), era_code.size()},
    year,
    {month_code.data(), month_code.size()},
    day,
    hour,
    minute,
    second,
    nanosecond,
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError>(diplomat::Ok<std::unique_ptr<icu4x::DateTime>>(std::unique_ptr<icu4x::DateTime>(icu4x::DateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError>(diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::DateTime> icu4x::DateTime::from_date_and_time(const icu4x::Date& date, const icu4x::Time& time) {
  auto result = icu4x::capi::icu4x_DateTime_from_date_and_time_mv1(date.AsFFI(),
    time.AsFFI());
  return std::unique_ptr<icu4x::DateTime>(icu4x::DateTime::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarParseError> icu4x::DateTime::from_string(std::string_view v) {
  auto result = icu4x::capi::icu4x_DateTime_from_string_mv1({v.data(), v.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarParseError>(diplomat::Ok<std::unique_ptr<icu4x::DateTime>>(std::unique_ptr<icu4x::DateTime>(icu4x::DateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::Date> icu4x::DateTime::date() const {
  auto result = icu4x::capi::icu4x_DateTime_date_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result));
}

inline std::unique_ptr<icu4x::Time> icu4x::DateTime::time() const {
  auto result = icu4x::capi::icu4x_DateTime_time_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(result));
}

inline std::unique_ptr<icu4x::IsoDateTime> icu4x::DateTime::to_iso() const {
  auto result = icu4x::capi::icu4x_DateTime_to_iso_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::IsoDateTime>(icu4x::IsoDateTime::FromFFI(result));
}

inline std::unique_ptr<icu4x::DateTime> icu4x::DateTime::to_calendar(const icu4x::Calendar& calendar) const {
  auto result = icu4x::capi::icu4x_DateTime_to_calendar_mv1(this->AsFFI(),
    calendar.AsFFI());
  return std::unique_ptr<icu4x::DateTime>(icu4x::DateTime::FromFFI(result));
}

inline uint8_t icu4x::DateTime::hour() const {
  auto result = icu4x::capi::icu4x_DateTime_hour_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::DateTime::minute() const {
  auto result = icu4x::capi::icu4x_DateTime_minute_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::DateTime::second() const {
  auto result = icu4x::capi::icu4x_DateTime_second_mv1(this->AsFFI());
  return result;
}

inline uint32_t icu4x::DateTime::nanosecond() const {
  auto result = icu4x::capi::icu4x_DateTime_nanosecond_mv1(this->AsFFI());
  return result;
}

inline uint16_t icu4x::DateTime::day_of_year() const {
  auto result = icu4x::capi::icu4x_DateTime_day_of_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::DateTime::day_of_month() const {
  auto result = icu4x::capi::icu4x_DateTime_day_of_month_mv1(this->AsFFI());
  return result;
}

inline icu4x::IsoWeekday icu4x::DateTime::day_of_week() const {
  auto result = icu4x::capi::icu4x_DateTime_day_of_week_mv1(this->AsFFI());
  return icu4x::IsoWeekday::FromFFI(result);
}

inline uint8_t icu4x::DateTime::week_of_month(icu4x::IsoWeekday first_weekday) const {
  auto result = icu4x::capi::icu4x_DateTime_week_of_month_mv1(this->AsFFI(),
    first_weekday.AsFFI());
  return result;
}

inline icu4x::WeekOf icu4x::DateTime::week_of_year(const icu4x::WeekCalculator& calculator) const {
  auto result = icu4x::capi::icu4x_DateTime_week_of_year_mv1(this->AsFFI(),
    calculator.AsFFI());
  return icu4x::WeekOf::FromFFI(result);
}

inline uint8_t icu4x::DateTime::ordinal_month() const {
  auto result = icu4x::capi::icu4x_DateTime_ordinal_month_mv1(this->AsFFI());
  return result;
}

inline std::string icu4x::DateTime::month_code() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_DateTime_month_code_mv1(this->AsFFI(),
    &write);
  return output;
}

inline uint8_t icu4x::DateTime::month_number() const {
  auto result = icu4x::capi::icu4x_DateTime_month_number_mv1(this->AsFFI());
  return result;
}

inline bool icu4x::DateTime::month_is_leap() const {
  auto result = icu4x::capi::icu4x_DateTime_month_is_leap_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::DateTime::year_in_era() const {
  auto result = icu4x::capi::icu4x_DateTime_year_in_era_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::DateTime::extended_year() const {
  auto result = icu4x::capi::icu4x_DateTime_extended_year_mv1(this->AsFFI());
  return result;
}

inline std::string icu4x::DateTime::era() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_DateTime_era_mv1(this->AsFFI(),
    &write);
  return output;
}

inline uint8_t icu4x::DateTime::months_in_year() const {
  auto result = icu4x::capi::icu4x_DateTime_months_in_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::DateTime::days_in_month() const {
  auto result = icu4x::capi::icu4x_DateTime_days_in_month_mv1(this->AsFFI());
  return result;
}

inline uint16_t icu4x::DateTime::days_in_year() const {
  auto result = icu4x::capi::icu4x_DateTime_days_in_year_mv1(this->AsFFI());
  return result;
}

inline std::unique_ptr<icu4x::Calendar> icu4x::DateTime::calendar() const {
  auto result = icu4x::capi::icu4x_DateTime_calendar_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Calendar>(icu4x::Calendar::FromFFI(result));
}

inline const icu4x::capi::DateTime* icu4x::DateTime::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::DateTime*>(this);
}

inline icu4x::capi::DateTime* icu4x::DateTime::AsFFI() {
  return reinterpret_cast<icu4x::capi::DateTime*>(this);
}

inline const icu4x::DateTime* icu4x::DateTime::FromFFI(const icu4x::capi::DateTime* ptr) {
  return reinterpret_cast<const icu4x::DateTime*>(ptr);
}

inline icu4x::DateTime* icu4x::DateTime::FromFFI(icu4x::capi::DateTime* ptr) {
  return reinterpret_cast<icu4x::DateTime*>(ptr);
}

inline void icu4x::DateTime::operator delete(void* ptr) {
  icu4x::capi::icu4x_DateTime_destroy_mv1(reinterpret_cast<icu4x::capi::DateTime*>(ptr));
}


#endif // icu4x_DateTime_HPP
