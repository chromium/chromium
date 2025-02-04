#ifndef icu4x_IsoDate_D_HPP
#define icu4x_IsoDate_D_HPP

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
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct WeekCalculator; }
class WeekCalculator;
struct WeekOf;
class CalendarError;
class CalendarParseError;
class IsoWeekday;
}


namespace icu4x {
namespace capi {
    struct IsoDate;
} // namespace capi
} // namespace

namespace icu4x {
class IsoDate {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarError> create(int32_t year, uint8_t month, uint8_t day);

  inline static diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarParseError> from_string(std::string_view v);

  inline std::unique_ptr<icu4x::Date> to_calendar(const icu4x::Calendar& calendar) const;

  inline std::unique_ptr<icu4x::Date> to_any() const;

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

  inline const icu4x::capi::IsoDate* AsFFI() const;
  inline icu4x::capi::IsoDate* AsFFI();
  inline static const icu4x::IsoDate* FromFFI(const icu4x::capi::IsoDate* ptr);
  inline static icu4x::IsoDate* FromFFI(icu4x::capi::IsoDate* ptr);
  inline static void operator delete(void* ptr);
private:
  IsoDate() = delete;
  IsoDate(const icu4x::IsoDate&) = delete;
  IsoDate(icu4x::IsoDate&&) noexcept = delete;
  IsoDate operator=(const icu4x::IsoDate&) = delete;
  IsoDate operator=(icu4x::IsoDate&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_IsoDate_D_HPP
