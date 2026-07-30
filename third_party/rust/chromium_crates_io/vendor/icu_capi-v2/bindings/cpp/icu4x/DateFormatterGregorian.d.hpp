#ifndef ICU4X_DateFormatterGregorian_D_HPP
#define ICU4X_DateFormatterGregorian_D_HPP

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
namespace capi { struct DateFormatterGregorian; }
class DateFormatterGregorian;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
class DateTimeAlignment;
class DateTimeFormatterLoadError;
class DateTimeLength;
class YearStyle;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateFormatterGregorian;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `FixedCalendarDateTimeFormatter`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html) for more information.
 */
class DateFormatterGregorian {
public:

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `D`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.D.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.D.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.D.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_d(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `D`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.D.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.D.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.D.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_d_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MD`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MD.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MD.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_md(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MD`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MD.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MD.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_md_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMD`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymd(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMD`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMD.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymd_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DE`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DE.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DE.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_de(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DE`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DE.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.DE.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_de_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDE`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDE.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDE.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDE`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDE.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.MDE.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDE`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDE`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YMDE.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `E`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.E.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.E.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_e(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `E`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.E.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.E.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_e_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `M`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.M.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.M.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.M.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_m(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `M`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.M.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.M.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.M.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_m_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YM`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ym(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YM`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.YM.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ym_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `Y`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_y(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `Y`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html#method.with_alignment), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html#method.with_year_style), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.Y.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_y_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/datetime/struct.FixedCalendarDateTimeFormatter.html#method.format) for more information.
   */
  inline std::string format_iso(const icu4x::IsoDate& iso_date) const;
  template<typename W>
  inline void format_iso_write(const icu4x::IsoDate& iso_date, W& writeable_output) const;

    inline const icu4x::capi::DateFormatterGregorian* AsFFI() const;
    inline icu4x::capi::DateFormatterGregorian* AsFFI();
    inline static const icu4x::DateFormatterGregorian* FromFFI(const icu4x::capi::DateFormatterGregorian* ptr);
    inline static icu4x::DateFormatterGregorian* FromFFI(icu4x::capi::DateFormatterGregorian* ptr);
    inline static void operator delete(void* ptr);
private:
    DateFormatterGregorian() = delete;
    DateFormatterGregorian(const icu4x::DateFormatterGregorian&) = delete;
    DateFormatterGregorian(icu4x::DateFormatterGregorian&&) noexcept = delete;
    DateFormatterGregorian operator=(const icu4x::DateFormatterGregorian&) = delete;
    DateFormatterGregorian operator=(icu4x::DateFormatterGregorian&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_DateFormatterGregorian_D_HPP
