#ifndef ICU4X_ZonedDateTimeFormatter_D_HPP
#define ICU4X_ZonedDateTimeFormatter_D_HPP

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
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Date; }
class Date;
namespace capi { struct DateTimeFormatter; }
class DateTimeFormatter;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct ZonedDateTimeFormatter; }
class ZonedDateTimeFormatter;
struct DateTimeMismatchedCalendarError;
class DateTimeFormatterLoadError;
class DateTimeWriteError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ZonedDateTimeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `DateTimeFormatter`](https://docs.rs/icu/2.2.0/icu/datetime/struct.DateTimeFormatter.html) for more information.
 */
class ZonedDateTimeFormatter {
public:

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_long(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_short(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_long(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_short(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_long(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_short(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `Location`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.Location.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_location(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `Location`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.Location.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_location_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `ExemplarCity`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.ExemplarCity.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_exemplar_city(const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `ExemplarCity`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.ExemplarCity.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_exemplar_city_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateTimeFormatter& formatter);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/datetime/struct.DateTimeFormatter.html#method.format) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError> format_iso(const icu4x::IsoDate& iso_date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError> format_iso_write(const icu4x::IsoDate& iso_date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone, W& writeable_output) const;

  /**
   * See the [Rust documentation for `format_same_calendar`](https://docs.rs/icu/2.2.0/icu/datetime/struct.DateTimeFormatter.html#method.format_same_calendar) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError> format_same_calendar(const icu4x::Date& date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError> format_same_calendar_write(const icu4x::Date& date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone, W& writeable_output) const;

    inline const icu4x::capi::ZonedDateTimeFormatter* AsFFI() const;
    inline icu4x::capi::ZonedDateTimeFormatter* AsFFI();
    inline static const icu4x::ZonedDateTimeFormatter* FromFFI(const icu4x::capi::ZonedDateTimeFormatter* ptr);
    inline static icu4x::ZonedDateTimeFormatter* FromFFI(icu4x::capi::ZonedDateTimeFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    ZonedDateTimeFormatter() = delete;
    ZonedDateTimeFormatter(const icu4x::ZonedDateTimeFormatter&) = delete;
    ZonedDateTimeFormatter(icu4x::ZonedDateTimeFormatter&&) noexcept = delete;
    ZonedDateTimeFormatter operator=(const icu4x::ZonedDateTimeFormatter&) = delete;
    ZonedDateTimeFormatter operator=(icu4x::ZonedDateTimeFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ZonedDateTimeFormatter_D_HPP
