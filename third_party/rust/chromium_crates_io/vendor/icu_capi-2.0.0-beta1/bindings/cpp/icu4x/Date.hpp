#ifndef icu4x_Date_HPP
#define icu4x_Date_HPP

#include "Date.d.hpp"

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
#include "IsoDate.hpp"
#include "IsoWeekday.hpp"
#include "WeekCalculator.hpp"
#include "WeekOf.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_Date_from_iso_in_calendar_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Date_from_iso_in_calendar_mv1_result;
    icu4x_Date_from_iso_in_calendar_mv1_result icu4x_Date_from_iso_in_calendar_mv1(int32_t year, uint8_t month, uint8_t day, const icu4x::capi::Calendar* calendar);
    
    typedef struct icu4x_Date_from_codes_in_calendar_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Date_from_codes_in_calendar_mv1_result;
    icu4x_Date_from_codes_in_calendar_mv1_result icu4x_Date_from_codes_in_calendar_mv1(diplomat::capi::DiplomatStringView era_code, int32_t year, diplomat::capi::DiplomatStringView month_code, uint8_t day, const icu4x::capi::Calendar* calendar);
    
    typedef struct icu4x_Date_from_string_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_Date_from_string_mv1_result;
    icu4x_Date_from_string_mv1_result icu4x_Date_from_string_mv1(diplomat::capi::DiplomatStringView v);
    
    icu4x::capi::Date* icu4x_Date_to_calendar_mv1(const icu4x::capi::Date* self, const icu4x::capi::Calendar* calendar);
    
    icu4x::capi::IsoDate* icu4x_Date_to_iso_mv1(const icu4x::capi::Date* self);
    
    uint16_t icu4x_Date_day_of_year_mv1(const icu4x::capi::Date* self);
    
    uint8_t icu4x_Date_day_of_month_mv1(const icu4x::capi::Date* self);
    
    icu4x::capi::IsoWeekday icu4x_Date_day_of_week_mv1(const icu4x::capi::Date* self);
    
    uint8_t icu4x_Date_week_of_month_mv1(const icu4x::capi::Date* self, icu4x::capi::IsoWeekday first_weekday);
    
    icu4x::capi::WeekOf icu4x_Date_week_of_year_mv1(const icu4x::capi::Date* self, const icu4x::capi::WeekCalculator* calculator);
    
    uint8_t icu4x_Date_ordinal_month_mv1(const icu4x::capi::Date* self);
    
    void icu4x_Date_month_code_mv1(const icu4x::capi::Date* self, diplomat::capi::DiplomatWrite* write);
    
    uint8_t icu4x_Date_month_number_mv1(const icu4x::capi::Date* self);
    
    bool icu4x_Date_month_is_leap_mv1(const icu4x::capi::Date* self);
    
    int32_t icu4x_Date_year_in_era_mv1(const icu4x::capi::Date* self);
    
    int32_t icu4x_Date_extended_year_mv1(const icu4x::capi::Date* self);
    
    void icu4x_Date_era_mv1(const icu4x::capi::Date* self, diplomat::capi::DiplomatWrite* write);
    
    uint8_t icu4x_Date_months_in_year_mv1(const icu4x::capi::Date* self);
    
    uint8_t icu4x_Date_days_in_month_mv1(const icu4x::capi::Date* self);
    
    uint16_t icu4x_Date_days_in_year_mv1(const icu4x::capi::Date* self);
    
    icu4x::capi::Calendar* icu4x_Date_calendar_mv1(const icu4x::capi::Date* self);
    
    
    void icu4x_Date_destroy_mv1(Date* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> icu4x::Date::from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, const icu4x::Calendar& calendar) {
  auto result = icu4x::capi::icu4x_Date_from_iso_in_calendar_mv1(year,
    month,
    day,
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> icu4x::Date::from_codes_in_calendar(std::string_view era_code, int32_t year, std::string_view month_code, uint8_t day, const icu4x::Calendar& calendar) {
  auto result = icu4x::capi::icu4x_Date_from_codes_in_calendar_mv1({era_code.data(), era_code.size()},
    year,
    {month_code.data(), month_code.size()},
    day,
    calendar.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarParseError> icu4x::Date::from_string(std::string_view v) {
  auto result = icu4x::capi::icu4x_Date_from_string_mv1({v.data(), v.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarParseError>(diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::Date> icu4x::Date::to_calendar(const icu4x::Calendar& calendar) const {
  auto result = icu4x::capi::icu4x_Date_to_calendar_mv1(this->AsFFI(),
    calendar.AsFFI());
  return std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result));
}

inline std::unique_ptr<icu4x::IsoDate> icu4x::Date::to_iso() const {
  auto result = icu4x::capi::icu4x_Date_to_iso_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::IsoDate>(icu4x::IsoDate::FromFFI(result));
}

inline uint16_t icu4x::Date::day_of_year() const {
  auto result = icu4x::capi::icu4x_Date_day_of_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::Date::day_of_month() const {
  auto result = icu4x::capi::icu4x_Date_day_of_month_mv1(this->AsFFI());
  return result;
}

inline icu4x::IsoWeekday icu4x::Date::day_of_week() const {
  auto result = icu4x::capi::icu4x_Date_day_of_week_mv1(this->AsFFI());
  return icu4x::IsoWeekday::FromFFI(result);
}

inline uint8_t icu4x::Date::week_of_month(icu4x::IsoWeekday first_weekday) const {
  auto result = icu4x::capi::icu4x_Date_week_of_month_mv1(this->AsFFI(),
    first_weekday.AsFFI());
  return result;
}

inline icu4x::WeekOf icu4x::Date::week_of_year(const icu4x::WeekCalculator& calculator) const {
  auto result = icu4x::capi::icu4x_Date_week_of_year_mv1(this->AsFFI(),
    calculator.AsFFI());
  return icu4x::WeekOf::FromFFI(result);
}

inline uint8_t icu4x::Date::ordinal_month() const {
  auto result = icu4x::capi::icu4x_Date_ordinal_month_mv1(this->AsFFI());
  return result;
}

inline std::string icu4x::Date::month_code() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_Date_month_code_mv1(this->AsFFI(),
    &write);
  return output;
}

inline uint8_t icu4x::Date::month_number() const {
  auto result = icu4x::capi::icu4x_Date_month_number_mv1(this->AsFFI());
  return result;
}

inline bool icu4x::Date::month_is_leap() const {
  auto result = icu4x::capi::icu4x_Date_month_is_leap_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::Date::year_in_era() const {
  auto result = icu4x::capi::icu4x_Date_year_in_era_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::Date::extended_year() const {
  auto result = icu4x::capi::icu4x_Date_extended_year_mv1(this->AsFFI());
  return result;
}

inline std::string icu4x::Date::era() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_Date_era_mv1(this->AsFFI(),
    &write);
  return output;
}

inline uint8_t icu4x::Date::months_in_year() const {
  auto result = icu4x::capi::icu4x_Date_months_in_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::Date::days_in_month() const {
  auto result = icu4x::capi::icu4x_Date_days_in_month_mv1(this->AsFFI());
  return result;
}

inline uint16_t icu4x::Date::days_in_year() const {
  auto result = icu4x::capi::icu4x_Date_days_in_year_mv1(this->AsFFI());
  return result;
}

inline std::unique_ptr<icu4x::Calendar> icu4x::Date::calendar() const {
  auto result = icu4x::capi::icu4x_Date_calendar_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Calendar>(icu4x::Calendar::FromFFI(result));
}

inline const icu4x::capi::Date* icu4x::Date::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::Date*>(this);
}

inline icu4x::capi::Date* icu4x::Date::AsFFI() {
  return reinterpret_cast<icu4x::capi::Date*>(this);
}

inline const icu4x::Date* icu4x::Date::FromFFI(const icu4x::capi::Date* ptr) {
  return reinterpret_cast<const icu4x::Date*>(ptr);
}

inline icu4x::Date* icu4x::Date::FromFFI(icu4x::capi::Date* ptr) {
  return reinterpret_cast<icu4x::Date*>(ptr);
}

inline void icu4x::Date::operator delete(void* ptr) {
  icu4x::capi::icu4x_Date_destroy_mv1(reinterpret_cast<icu4x::capi::Date*>(ptr));
}


#endif // icu4x_Date_HPP
