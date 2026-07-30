#ifndef ICU4X_TimeZoneFormatter_D_HPP
#define ICU4X_TimeZoneFormatter_D_HPP

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
namespace capi { struct Locale; }
class Locale;
namespace capi { struct TimeZoneFormatter; }
class TimeZoneFormatter;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
class DateTimeFormatterLoadError;
class DateTimeWriteError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeZoneFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `NoCalendarFormatter`](https://docs.rs/icu/2.2.0/icu/datetime/type.NoCalendarFormatter.html) for more information.
 */
class TimeZoneFormatter {
public:

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_long(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_short(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `SpecificShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.SpecificShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_specific_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_long(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_short(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `LocalizedOffsetShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.LocalizedOffsetShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_localized_offset_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_long(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericLong`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericLong.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_short(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `GenericShort`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.GenericShort.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_generic_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `Location`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.Location.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_location(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `Location`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.Location.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_location_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `ExemplarCity`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.ExemplarCity.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_exemplar_city(const icu4x::Locale& locale);

  /**
   * Creates a zoned formatter based on a non-zoned formatter.
   *
   * Caution: The locale provided here must match the locale used to construct the non-zoned formatter,
   * or else unexpected behavior may occur!
   *
   * See the [Rust documentation for `ExemplarCity`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/zone/struct.ExemplarCity.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> create_exemplar_city_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.format) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError> format(const icu4x::TimeZoneInfo& zone) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError> format_write(const icu4x::TimeZoneInfo& zone, W& writeable_output) const;

    inline const icu4x::capi::TimeZoneFormatter* AsFFI() const;
    inline icu4x::capi::TimeZoneFormatter* AsFFI();
    inline static const icu4x::TimeZoneFormatter* FromFFI(const icu4x::capi::TimeZoneFormatter* ptr);
    inline static icu4x::TimeZoneFormatter* FromFFI(icu4x::capi::TimeZoneFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    TimeZoneFormatter() = delete;
    TimeZoneFormatter(const icu4x::TimeZoneFormatter&) = delete;
    TimeZoneFormatter(icu4x::TimeZoneFormatter&&) noexcept = delete;
    TimeZoneFormatter operator=(const icu4x::TimeZoneFormatter&) = delete;
    TimeZoneFormatter operator=(icu4x::TimeZoneFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TimeZoneFormatter_D_HPP
