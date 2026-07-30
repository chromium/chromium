#ifndef ICU4X_DateTimeFormatterGregorian_D_HPP
#define ICU4X_DateTimeFormatterGregorian_D_HPP

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
namespace capi { struct DateTimeFormatterGregorian; }
class DateTimeFormatterGregorian;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct Time; }
class Time;
class DateTimeAlignment;
class DateTimeFormatterLoadError;
class DateTimeLength;
class TimePrecision;
class YearStyle;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateTimeFormatterGregorian;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `FixedCalendarDateTimeFormatter`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html) for more information.
 */
class DateTimeFormatterGregorian {
public:

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DT`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_dt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DT`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DT.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_dt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDT`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDT`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDT.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDT`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_year_style), [4](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDT`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_year_style), [4](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDT.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_det(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_det_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_year_style), [4](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_year_style), [4](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `ET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_et(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `ET`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.ET.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_et_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.format) for more information.
   */
  inline std::string format_iso(const icu4x::IsoDate& iso_date, const icu4x::Time& time) const;
  template<typename W>
  inline void format_iso_write(const icu4x::IsoDate& iso_date, const icu4x::Time& time, W& writeable_output) const;

    inline const icu4x::capi::DateTimeFormatterGregorian* AsFFI() const;
    inline icu4x::capi::DateTimeFormatterGregorian* AsFFI();
    inline static const icu4x::DateTimeFormatterGregorian* FromFFI(const icu4x::capi::DateTimeFormatterGregorian* ptr);
    inline static icu4x::DateTimeFormatterGregorian* FromFFI(icu4x::capi::DateTimeFormatterGregorian* ptr);
    inline static void operator delete(void* ptr);
private:
    DateTimeFormatterGregorian() = delete;
    DateTimeFormatterGregorian(const icu4x::DateTimeFormatterGregorian&) = delete;
    DateTimeFormatterGregorian(icu4x::DateTimeFormatterGregorian&&) noexcept = delete;
    DateTimeFormatterGregorian operator=(const icu4x::DateTimeFormatterGregorian&) = delete;
    DateTimeFormatterGregorian operator=(icu4x::DateTimeFormatterGregorian&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_DateTimeFormatterGregorian_D_HPP
