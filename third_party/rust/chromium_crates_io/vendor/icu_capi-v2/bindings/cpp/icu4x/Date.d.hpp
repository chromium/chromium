#ifndef icu4x_Date_D_HPP
#define icu4x_Date_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Date; }
class Date;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct WeekCalculator; }
class WeekCalculator;
struct WeekOf;
class CalendarError;
class CalendarParseError;
class Weekday;
}


namespace icu4x {
namespace capi {
    struct Date;
} // namespace capi
} // namespace

namespace icu4x {
class Date {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, const icu4x::Calendar& calendar);

  inline static diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> from_codes_in_calendar(std::string_view era_code, int32_t year, std::string_view month_code, uint8_t day, const icu4x::Calendar& calendar);

  inline static diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarParseError> from_string(std::string_view v, const icu4x::Calendar& calendar);

  inline std::unique_ptr<icu4x::Date> to_calendar(const icu4x::Calendar& calendar) const;

  inline std::unique_ptr<icu4x::IsoDate> to_iso() const;

  inline uint16_t day_of_year() const;

  inline uint8_t day_of_month() const;

  inline icu4x::Weekday day_of_week() const;

  inline uint8_t week_of_month(icu4x::Weekday first_weekday) const;

  inline icu4x::WeekOf week_of_year(const icu4x::WeekCalculator& calculator) const;

  inline uint8_t ordinal_month() const;

  inline std::string month_code() const;

  inline uint8_t month_number() const;

  inline bool month_is_leap() const;

  inline int32_t year_in_era() const;

  inline int32_t extended_year() const;

  inline std::string era() const;

  inline uint8_t months_in_year() const;

  inline uint8_t days_in_month() const;

  inline uint16_t days_in_year() const;

  inline std::unique_ptr<icu4x::Calendar> calendar() const;

  inline const icu4x::capi::Date* AsFFI() const;
  inline icu4x::capi::Date* AsFFI();
  inline static const icu4x::Date* FromFFI(const icu4x::capi::Date* ptr);
  inline static icu4x::Date* FromFFI(icu4x::capi::Date* ptr);
  inline static void operator delete(void* ptr);
private:
  Date() = delete;
  Date(const icu4x::Date&) = delete;
  Date(icu4x::Date&&) noexcept = delete;
  Date operator=(const icu4x::Date&) = delete;
  Date operator=(icu4x::Date&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_Date_D_HPP
