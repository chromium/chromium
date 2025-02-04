#ifndef icu4x_DateTime_D_HPP
#define icu4x_DateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Date; }
class Date;
namespace capi { struct DateTime; }
class DateTime;
namespace capi { struct IsoDateTime; }
class IsoDateTime;
namespace capi { struct Time; }
class Time;
namespace capi { struct WeekCalculator; }
class WeekCalculator;
struct WeekOf;
class CalendarError;
class CalendarParseError;
class IsoWeekday;
}


namespace icu4x {
namespace capi {
    struct DateTime;
} // namespace capi
} // namespace

namespace icu4x {
class DateTime {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError> from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const icu4x::Calendar& calendar);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarError> from_codes_in_calendar(std::string_view era_code, int32_t year, std::string_view month_code, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const icu4x::Calendar& calendar);

  inline static std::unique_ptr<icu4x::DateTime> from_date_and_time(const icu4x::Date& date, const icu4x::Time& time);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTime>, icu4x::CalendarParseError> from_string(std::string_view v);

  inline std::unique_ptr<icu4x::Date> date() const;

  inline std::unique_ptr<icu4x::Time> time() const;

  inline std::unique_ptr<icu4x::IsoDateTime> to_iso() const;

  inline std::unique_ptr<icu4x::DateTime> to_calendar(const icu4x::Calendar& calendar) const;

  inline uint8_t hour() const;

  inline uint8_t minute() const;

  inline uint8_t second() const;

  inline uint32_t nanosecond() const;

  inline uint16_t day_of_year() const;

  inline uint8_t day_of_month() const;

  inline icu4x::IsoWeekday day_of_week() const;

  inline uint8_t week_of_month(icu4x::IsoWeekday first_weekday) const;

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

  inline const icu4x::capi::DateTime* AsFFI() const;
  inline icu4x::capi::DateTime* AsFFI();
  inline static const icu4x::DateTime* FromFFI(const icu4x::capi::DateTime* ptr);
  inline static icu4x::DateTime* FromFFI(icu4x::capi::DateTime* ptr);
  inline static void operator delete(void* ptr);
private:
  DateTime() = delete;
  DateTime(const icu4x::DateTime&) = delete;
  DateTime(icu4x::DateTime&&) noexcept = delete;
  DateTime operator=(const icu4x::DateTime&) = delete;
  DateTime operator=(icu4x::DateTime&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_DateTime_D_HPP
