#ifndef ICU4X_DateTimeRangeFormatterGregorian_D_HPP
#define ICU4X_DateTimeRangeFormatterGregorian_D_HPP

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
namespace capi { struct DateTimeRangeFormatterGregorian; }
class DateTimeRangeFormatterGregorian;
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
    struct DateTimeRangeFormatterGregorian;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `FixedCalendarDateRangeFormatter`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html) for more information.
 *
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 */
class DateTimeRangeFormatterGregorian {
public:

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DT`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_dt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DT`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DT.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_dt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDT`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDT`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDT.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDT`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_year_style), [4](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDT`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.with_year_style), [4](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDT.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_det(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_det_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_year_style), [4](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.with_year_style), [4](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `ET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_et(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `ET`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.ET.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeRangeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_et_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.FixedCalendarDateRangeFormatter.html#method.format) for more information.
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline std::string format_iso(const icu4x::IsoDate& start_iso_date, const icu4x::Time& start_time, const icu4x::IsoDate& end_iso_date, const icu4x::Time& end_time) const;
  template<typename W>
  inline void format_iso_write(const icu4x::IsoDate& start_iso_date, const icu4x::Time& start_time, const icu4x::IsoDate& end_iso_date, const icu4x::Time& end_time, W& writeable_output) const;

    inline const icu4x::capi::DateTimeRangeFormatterGregorian* AsFFI() const;
    inline icu4x::capi::DateTimeRangeFormatterGregorian* AsFFI();
    inline static const icu4x::DateTimeRangeFormatterGregorian* FromFFI(const icu4x::capi::DateTimeRangeFormatterGregorian* ptr);
    inline static icu4x::DateTimeRangeFormatterGregorian* FromFFI(icu4x::capi::DateTimeRangeFormatterGregorian* ptr);
    inline static void operator delete(void* ptr);
private:
    DateTimeRangeFormatterGregorian() = delete;
    DateTimeRangeFormatterGregorian(const icu4x::DateTimeRangeFormatterGregorian&) = delete;
    DateTimeRangeFormatterGregorian(icu4x::DateTimeRangeFormatterGregorian&&) noexcept = delete;
    DateTimeRangeFormatterGregorian operator=(const icu4x::DateTimeRangeFormatterGregorian&) = delete;
    DateTimeRangeFormatterGregorian operator=(icu4x::DateTimeRangeFormatterGregorian&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_DateTimeRangeFormatterGregorian_D_HPP
