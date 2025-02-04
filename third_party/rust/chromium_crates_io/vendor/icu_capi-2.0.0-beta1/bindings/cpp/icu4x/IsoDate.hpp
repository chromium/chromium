#ifndef icu4x_IsoDate_HPP
#define icu4x_IsoDate_HPP

#include "IsoDate.d.hpp"

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
#include "IsoWeekday.hpp"
#include "WeekCalculator.hpp"
#include "WeekOf.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_IsoDate_create_mv1_result {union {icu4x::capi::IsoDate* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_IsoDate_create_mv1_result;
    icu4x_IsoDate_create_mv1_result icu4x_IsoDate_create_mv1(int32_t year, uint8_t month, uint8_t day);
    
    typedef struct icu4x_IsoDate_from_string_mv1_result {union {icu4x::capi::IsoDate* ok; icu4x::capi::CalendarParseError err;}; bool is_ok;} icu4x_IsoDate_from_string_mv1_result;
    icu4x_IsoDate_from_string_mv1_result icu4x_IsoDate_from_string_mv1(diplomat::capi::DiplomatStringView v);
    
    icu4x::capi::Date* icu4x_IsoDate_to_calendar_mv1(const icu4x::capi::IsoDate* self, const icu4x::capi::Calendar* calendar);
    
    icu4x::capi::Date* icu4x_IsoDate_to_any_mv1(const icu4x::capi::IsoDate* self);
    
    uint16_t icu4x_IsoDate_day_of_year_mv1(const icu4x::capi::IsoDate* self);
    
    uint8_t icu4x_IsoDate_day_of_month_mv1(const icu4x::capi::IsoDate* self);
    
    icu4x::capi::IsoWeekday icu4x_IsoDate_day_of_week_mv1(const icu4x::capi::IsoDate* self);
    
    uint8_t icu4x_IsoDate_week_of_month_mv1(const icu4x::capi::IsoDate* self, icu4x::capi::IsoWeekday first_weekday);
    
    icu4x::capi::WeekOf icu4x_IsoDate_week_of_year_mv1(const icu4x::capi::IsoDate* self, const icu4x::capi::WeekCalculator* calculator);
    
    uint8_t icu4x_IsoDate_month_mv1(const icu4x::capi::IsoDate* self);
    
    int32_t icu4x_IsoDate_year_mv1(const icu4x::capi::IsoDate* self);
    
    bool icu4x_IsoDate_is_in_leap_year_mv1(const icu4x::capi::IsoDate* self);
    
    uint8_t icu4x_IsoDate_months_in_year_mv1(const icu4x::capi::IsoDate* self);
    
    uint8_t icu4x_IsoDate_days_in_month_mv1(const icu4x::capi::IsoDate* self);
    
    uint16_t icu4x_IsoDate_days_in_year_mv1(const icu4x::capi::IsoDate* self);
    
    
    void icu4x_IsoDate_destroy_mv1(IsoDate* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarError> icu4x::IsoDate::create(int32_t year, uint8_t month, uint8_t day) {
  auto result = icu4x::capi::icu4x_IsoDate_create_mv1(year,
    month,
    day);
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarError>(diplomat::Ok<std::unique_ptr<icu4x::IsoDate>>(std::unique_ptr<icu4x::IsoDate>(icu4x::IsoDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarError>(diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarParseError> icu4x::IsoDate::from_string(std::string_view v) {
  auto result = icu4x::capi::icu4x_IsoDate_from_string_mv1({v.data(), v.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarParseError>(diplomat::Ok<std::unique_ptr<icu4x::IsoDate>>(std::unique_ptr<icu4x::IsoDate>(icu4x::IsoDate::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarParseError>(diplomat::Err<icu4x::CalendarParseError>(icu4x::CalendarParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::Date> icu4x::IsoDate::to_calendar(const icu4x::Calendar& calendar) const {
  auto result = icu4x::capi::icu4x_IsoDate_to_calendar_mv1(this->AsFFI(),
    calendar.AsFFI());
  return std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result));
}

inline std::unique_ptr<icu4x::Date> icu4x::IsoDate::to_any() const {
  auto result = icu4x::capi::icu4x_IsoDate_to_any_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result));
}

inline uint16_t icu4x::IsoDate::day_of_year() const {
  auto result = icu4x::capi::icu4x_IsoDate_day_of_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDate::day_of_month() const {
  auto result = icu4x::capi::icu4x_IsoDate_day_of_month_mv1(this->AsFFI());
  return result;
}

inline icu4x::IsoWeekday icu4x::IsoDate::day_of_week() const {
  auto result = icu4x::capi::icu4x_IsoDate_day_of_week_mv1(this->AsFFI());
  return icu4x::IsoWeekday::FromFFI(result);
}

inline uint8_t icu4x::IsoDate::week_of_month(icu4x::IsoWeekday first_weekday) const {
  auto result = icu4x::capi::icu4x_IsoDate_week_of_month_mv1(this->AsFFI(),
    first_weekday.AsFFI());
  return result;
}

inline icu4x::WeekOf icu4x::IsoDate::week_of_year(const icu4x::WeekCalculator& calculator) const {
  auto result = icu4x::capi::icu4x_IsoDate_week_of_year_mv1(this->AsFFI(),
    calculator.AsFFI());
  return icu4x::WeekOf::FromFFI(result);
}

inline uint8_t icu4x::IsoDate::month() const {
  auto result = icu4x::capi::icu4x_IsoDate_month_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::IsoDate::year() const {
  auto result = icu4x::capi::icu4x_IsoDate_year_mv1(this->AsFFI());
  return result;
}

inline bool icu4x::IsoDate::is_in_leap_year() const {
  auto result = icu4x::capi::icu4x_IsoDate_is_in_leap_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDate::months_in_year() const {
  auto result = icu4x::capi::icu4x_IsoDate_months_in_year_mv1(this->AsFFI());
  return result;
}

inline uint8_t icu4x::IsoDate::days_in_month() const {
  auto result = icu4x::capi::icu4x_IsoDate_days_in_month_mv1(this->AsFFI());
  return result;
}

inline uint16_t icu4x::IsoDate::days_in_year() const {
  auto result = icu4x::capi::icu4x_IsoDate_days_in_year_mv1(this->AsFFI());
  return result;
}

inline const icu4x::capi::IsoDate* icu4x::IsoDate::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::IsoDate*>(this);
}

inline icu4x::capi::IsoDate* icu4x::IsoDate::AsFFI() {
  return reinterpret_cast<icu4x::capi::IsoDate*>(this);
}

inline const icu4x::IsoDate* icu4x::IsoDate::FromFFI(const icu4x::capi::IsoDate* ptr) {
  return reinterpret_cast<const icu4x::IsoDate*>(ptr);
}

inline icu4x::IsoDate* icu4x::IsoDate::FromFFI(icu4x::capi::IsoDate* ptr) {
  return reinterpret_cast<icu4x::IsoDate*>(ptr);
}

inline void icu4x::IsoDate::operator delete(void* ptr) {
  icu4x::capi::icu4x_IsoDate_destroy_mv1(reinterpret_cast<icu4x::capi::IsoDate*>(ptr));
}


#endif // icu4x_IsoDate_HPP
