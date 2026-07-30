#ifndef ICU4X_ZonedDateFormatter_D_HPP
#define ICU4X_ZonedDateFormatter_D_HPP

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
namespace capi { struct DateFormatter; }
class DateFormatter;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct ZonedDateFormatter; }
class ZonedDateFormatter;
struct DateTimeMismatchedCalendarError;
class DateTimeFormatterLoadError;
class DateTimeWriteError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ZonedDateFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `DateTimeFormatter`](https://docs.rs/icu/2.2.0/icu/datetime/struct.DateTimeFormatter.html) for more information.
 */
class ZonedDateFormatter {
public:

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_long(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_short(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_long(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_short(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_long(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_short(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `Location`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.Location.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_location(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `Location`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.Location.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_location_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `ExemplarCity`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.ExemplarCity.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_exemplar_city(const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `ExemplarCity`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.ExemplarCity.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedDateFormatter>, icu4x::DateTimeFormatterLoadError> create_exemplar_city_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::DateFormatter& formatter);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/datetime/struct.DateTimeFormatter.html#method.format) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError> format_iso(const icu4x::IsoDate& iso_date, const icu4x::TimeZoneInfo& zone) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError> format_iso_write(const icu4x::IsoDate& iso_date, const icu4x::TimeZoneInfo& zone, W& writeable_output) const;

  /**
   * See the [Rust documentation for `format_same_calendar`](https://docs.rs/icu/2.2.0/icu/datetime/struct.DateTimeFormatter.html#method.format_same_calendar) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError> format_same_calendar(const icu4x::Date& date, const icu4x::TimeZoneInfo& zone) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError> format_same_calendar_write(const icu4x::Date& date, const icu4x::TimeZoneInfo& zone, W& writeable_output) const;

    inline const icu4x::capi::ZonedDateFormatter* AsFFI() const;
    inline icu4x::capi::ZonedDateFormatter* AsFFI();
    inline static const icu4x::ZonedDateFormatter* FromFFI(const icu4x::capi::ZonedDateFormatter* ptr);
    inline static icu4x::ZonedDateFormatter* FromFFI(icu4x::capi::ZonedDateFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    ZonedDateFormatter() = delete;
    ZonedDateFormatter(const icu4x::ZonedDateFormatter&) = delete;
    ZonedDateFormatter(icu4x::ZonedDateFormatter&&) noexcept = delete;
    ZonedDateFormatter operator=(const icu4x::ZonedDateFormatter&) = delete;
    ZonedDateFormatter operator=(icu4x::ZonedDateFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ZonedDateFormatter_D_HPP
