#ifndef icu4x_IsoDateTime_D_HPP
#define icu4x_IsoDateTime_D_HPP

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
namespace capi { struct DateTime; }
class DateTime;
namespace capi { struct IsoDate; }
class IsoDate;
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
    struct IsoDateTime;
} // namespace capi
} // namespace

namespace icu4x {
class IsoDateTime {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarError> create(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond);

  inline static std::unique_ptr<icu4x::IsoDateTime> from_date_and_time(const icu4x::IsoDate& date, const icu4x::Time& time);

  inline static diplomat::result<std::unique_ptr<icu4x::IsoDateTime>, icu4x::CalendarParseError> from_string(std::string_view v);

  inline std::unique_ptr<icu4x::IsoDate> date() const;

  inline std::unique_ptr<icu4x::Time> time() const;

  inline std::unique_ptr<icu4x::DateTime> to_any() const;

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

  inline uint8_t month() const;

  inline int32_t year() const;

  inline bool is_in_leap_year() const;

  inline uint8_t months_in_year() const;

  inline uint8_t days_in_month() const;

  inline uint16_t days_in_year() const;

  inline const icu4x::capi::IsoDateTime* AsFFI() const;
  inline icu4x::capi::IsoDateTime* AsFFI();
  inline static const icu4x::IsoDateTime* FromFFI(const icu4x::capi::IsoDateTime* ptr);
  inline static icu4x::IsoDateTime* FromFFI(icu4x::capi::IsoDateTime* ptr);
  inline static void operator delete(void* ptr);
private:
  IsoDateTime() = delete;
  IsoDateTime(const icu4x::IsoDateTime&) = delete;
  IsoDateTime(icu4x::IsoDateTime&&) noexcept = delete;
  IsoDateTime operator=(const icu4x::IsoDateTime&) = delete;
  IsoDateTime operator=(icu4x::IsoDateTime&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_IsoDateTime_D_HPP
