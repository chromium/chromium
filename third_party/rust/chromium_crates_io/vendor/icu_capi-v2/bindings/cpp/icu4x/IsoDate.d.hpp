#ifndef ICU4X_IsoDate_D_HPP
#define ICU4X_IsoDate_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Date; }
class Date;
namespace capi { struct IsoDate; }
class IsoDate;
struct DateAddOptions;
struct DateDifferenceOptions;
struct DateDuration;
struct IsoWeekOfYear;
class CalendarDateAddError;
class CalendarError;
class Rfc9557ParseError;
class Weekday;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct IsoDate;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Date object capable of containing a ISO-8601 date
 *
 * See the [Rust documentation for `Date`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html) for more information.
 */
class IsoDate {
public:

  /**
   * Creates a new {@link IsoDate} from the specified date.
   *
   * See the [Rust documentation for `try_new_iso`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_new_iso) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarError> create(int32_t year, uint8_t month, uint8_t day);

  /**
   * Creates a new {@link IsoDate} from the given Rata Die
   *
   * See the [Rust documentation for `from_rata_die`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.from_rata_die) for more information.
   */
  inline static std::unique_ptr<icu4x::IsoDate> from_rata_die(int64_t rd);

  /**
   * Creates a new {@link IsoDate} from an IXDTF string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::Rfc9557ParseError> from_string(std::string_view v);

  /**
   * Convert this date to one in a different calendar
   *
   * See the [Rust documentation for `to_calendar`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.to_calendar) for more information.
   */
  inline std::unique_ptr<icu4x::Date> to_calendar(const icu4x::Calendar& calendar) const;

  /**
   * See the [Rust documentation for `to_any`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.to_any) for more information.
   */
  inline std::unique_ptr<icu4x::Date> to_any() const;

  /**
   * Returns this date's Rata Die
   *
   * See the [Rust documentation for `to_rata_die`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.to_rata_die) for more information.
   */
  inline int64_t to_rata_die() const;

  /**
   * Returns the 1-indexed day in the year for this date
   *
   * See the [Rust documentation for `day_of_year`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.day_of_year) for more information.
   */
  inline uint16_t day_of_year() const;

  /**
   * Returns the 1-indexed day in the month for this date
   *
   * See the [Rust documentation for `day_of_month`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.day_of_month) for more information.
   */
  inline uint8_t day_of_month() const;

  /**
   * Returns the day in the week for this day
   *
   * This is *not* the day of the week, an ordinal number that is locale
   * dependent.
   *
   * See the [Rust documentation for `day_of_week`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.day_of_week) for more information.
   *
   * \deprecated use `weekday`
   */
  [[deprecated("use `weekday`")]]
  inline icu4x::Weekday day_of_week() const;

  /**
   * Returns the day in the week for this day
   *
   * See the [Rust documentation for `weekday`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.weekday) for more information.
   */
  inline icu4x::Weekday weekday() const;

  /**
   * Returns the week number in this year, using week data
   *
   * See the [Rust documentation for `week_of_year`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.week_of_year) for more information.
   */
  inline icu4x::IsoWeekOfYear week_of_year() const;

  /**
   * Returns 1-indexed number of the month of this date in its year
   *
   * See the [Rust documentation for `ordinal`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.MonthInfo.html#structfield.ordinal) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.month)
   */
  inline uint8_t month() const;

  /**
   * Returns the year number in the current era for this date
   *
   * For calendars without an era, returns the extended year
   *
   * See the [Rust documentation for `extended_year`](https://docs.rs/icu/2.2.0/icu/calendar/types/enum.YearInfo.html#method.extended_year) for more information.
   */
  inline int32_t year() const;

  /**
   * Returns if the year is a leap year for this date
   *
   * See the [Rust documentation for `is_in_leap_year`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.is_in_leap_year) for more information.
   */
  inline bool is_in_leap_year() const;

  /**
   * Returns the number of months in the year represented by this date
   *
   * See the [Rust documentation for `months_in_year`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.months_in_year) for more information.
   */
  inline uint8_t months_in_year() const;

  /**
   * Returns the number of days in the month represented by this date
   *
   * See the [Rust documentation for `days_in_month`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.days_in_month) for more information.
   */
  inline uint8_t days_in_month() const;

  /**
   * Returns the number of days in the year represented by this date
   *
   * See the [Rust documentation for `days_in_year`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.days_in_year) for more information.
   */
  inline uint16_t days_in_year() const;

  /**
   * Returns a new {@link IsoDate} with the given duration added to it.
   *
   * See the [Rust documentation for `try_added_with_options`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_added_with_options) for more information.
   */
  inline icu4x::diplomat::result<std::unique_ptr<icu4x::IsoDate>, icu4x::CalendarDateAddError> try_add_with_options(icu4x::DateDuration duration, icu4x::DateAddOptions options) const;

  /**
   * Calculating the duration between `other - self`
   *
   * See the [Rust documentation for `try_until_with_options`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_until_with_options) for more information.
   */
  inline icu4x::DateDuration until_with_options(const icu4x::IsoDate& other, icu4x::DateDifferenceOptions options) const;

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
#endif // ICU4X_IsoDate_D_HPP
