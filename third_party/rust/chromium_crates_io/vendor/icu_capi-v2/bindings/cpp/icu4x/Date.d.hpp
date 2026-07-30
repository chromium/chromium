#ifndef ICU4X_Date_D_HPP
#define ICU4X_Date_D_HPP

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
struct CalendarMismatchedCalendarError;
struct DateAddOptions;
struct DateDifferenceOptions;
struct DateDuration;
struct DateFields;
struct DateFromFieldsOptions;
class CalendarDateAddError;
class CalendarDateFromFieldsError;
class CalendarError;
class Rfc9557ParseError;
class Weekday;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct Date;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Date object capable of containing a date for any calendar.
 *
 * See the [Rust documentation for `Date`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html) for more information.
 */
class Date {
public:

  /**
   * Creates a new {@link Date} representing the ISO date
   * given but in a given calendar
   *
   * See the [Rust documentation for `new_from_iso`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.new_from_iso) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> from_iso_in_calendar(int32_t iso_year, uint8_t iso_month, uint8_t iso_day, const icu4x::Calendar& calendar);

  /**
   * Creates a new {@link Date} from the given fields, which are interpreted in the given calendar system.
   *
   * See the [Rust documentation for `try_from_fields`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_from_fields) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateFromFieldsError> from_fields_in_calendar(icu4x::DateFields fields, icu4x::DateFromFieldsOptions options, const icu4x::Calendar& calendar);

  /**
   * Creates a new {@link Date} from the given codes, which are interpreted in the given calendar system
   *
   * An empty era code will treat the year as an extended year
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.Month.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> from_codes_in_calendar(std::string_view era_code, int32_t year, std::string_view month_code, uint8_t day, const icu4x::Calendar& calendar);

  /**
   * Creates a new {@link Date} from the given Rata Die
   *
   * See the [Rust documentation for `from_rata_die`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.from_rata_die) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> from_rata_die(int64_t rd, const icu4x::Calendar& calendar);

  /**
   * Creates a new {@link Date} from an IXDTF string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::Rfc9557ParseError> from_string(std::string_view v, const icu4x::Calendar& calendar);

  /**
   * Convert this date to one in a different calendar
   *
   * See the [Rust documentation for `to_calendar`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.to_calendar) for more information.
   */
  inline std::unique_ptr<icu4x::Date> to_calendar(const icu4x::Calendar& calendar) const;

  /**
   * Converts this date to ISO
   *
   * See the [Rust documentation for `to_iso`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.to_iso) for more information.
   */
  inline std::unique_ptr<icu4x::IsoDate> to_iso() const;

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
   * Returns 1-indexed number of the month of this date in its year
   *
   * Note that for lunar calendars this may not lead to the same month
   * having the same ordinal month across years; use `month_code` if you care
   * about month identity.
   *
   * See the [Rust documentation for `month`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.month) for more information.
   *
   * See the [Rust documentation for `ordinal`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.MonthInfo.html#structfield.ordinal) for more information.
   */
  inline uint8_t ordinal_month() const;

  /**
   * Returns the month code for this date. Typically something
   * like "M01", "M02", but can be more complicated for lunar calendars.
   *
   * See the [Rust documentation for `code`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.Month.html#method.code) for more information.
   *
   * See the [Rust documentation for `standard_code`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.MonthInfo.html#structfield.standard_code) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.month)
   */
  inline std::string month_code() const;
  template<typename W>
  inline void month_code_write(W& writeable_output) const;

  /**
   * Returns the month number of this month.
   *
   * See the [Rust documentation for `number`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.Month.html#method.number) for more information.
   */
  inline uint8_t month_number() const;

  /**
   * Returns whether the month is a leap month.
   *
   * See the [Rust documentation for `is_leap`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.Month.html#method.is_leap) for more information.
   */
  inline bool month_is_leap() const;

  /**
   * Returns the year number in the current era for this date
   *
   * For calendars without an era, returns the related ISO year.
   *
   * See the [Rust documentation for `era_year_or_related_iso`](https://docs.rs/icu/2.2.0/icu/calendar/types/enum.YearInfo.html#method.era_year_or_related_iso) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.EraYear.html#structfield.year), [2](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.CyclicYear.html#structfield.related_iso), [3](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.year)
   */
  inline int32_t era_year_or_related_iso() const;

  /**
   * Returns the extended year, which can be used for
   *
   * This year number can be used when you need a simple numeric representation
   * of the year, and can be meaningfully compared with extended years from other
   * eras or used in arithmetic.
   *
   * See the [Rust documentation for `extended_year`](https://docs.rs/icu/2.2.0/icu/calendar/types/enum.YearInfo.html#method.extended_year) for more information.
   */
  inline int32_t extended_year() const;

  /**
   * Returns the era for this date, or an empty string
   *
   * See the [Rust documentation for `era`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.EraYear.html#structfield.era) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.year)
   */
  inline std::string era() const;
  template<typename W>
  inline void era_write(W& writeable_output) const;

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
   * Returns if the year is a leap year for this date
   *
   * See the [Rust documentation for `is_in_leap_year`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.is_in_leap_year) for more information.
   */
  inline bool is_in_leap_year() const;

  /**
   * Returns the {@link Calendar} object backing this date
   *
   * See the [Rust documentation for `calendar`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.calendar) for more information.
   */
  inline std::unique_ptr<icu4x::Calendar> calendar() const;

  /**
   * Returns a new {@link Date} with the given duration added to it.
   *
   * See the [Rust documentation for `try_added_with_options`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_added_with_options) for more information.
   */
  inline icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateAddError> try_add_with_options(icu4x::DateDuration duration, icu4x::DateAddOptions options) const;

  /**
   * Calculating the duration between `other - self`
   *
   * See the [Rust documentation for `try_until_with_options`](https://docs.rs/icu/2.2.0/icu/calendar/struct.Date.html#method.try_until_with_options) for more information.
   */
  inline icu4x::diplomat::result<icu4x::DateDuration, icu4x::CalendarMismatchedCalendarError> try_until_with_options(const icu4x::Date& other, icu4x::DateDifferenceOptions options) const;

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
#endif // ICU4X_Date_D_HPP
