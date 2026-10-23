#ifndef ICU4X_DateRangeFormatter_D_HPP
#define ICU4X_DateRangeFormatter_D_HPP

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
namespace capi { struct DateRangeFormatter; }
class DateRangeFormatter;
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
    struct DateRangeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `DateRangeFormatter`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html) for more information.
 *
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 */
class DateRangeFormatter {
public:

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `D`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.D.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.D.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.D.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_d(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `D`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.D.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.D.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.D.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_d_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MD`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MD.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MD.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_md(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MD`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MD.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MD.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_md_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMD`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_ymd(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMD`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMD.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_ymd_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DE`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DE.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DE.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_de(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `DE`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DE.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.DE.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_de_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDE`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDE.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDE.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_mde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `MDE`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDE.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.MDE.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_mde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDE`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_ymde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YMDE`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YMDE.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_ymde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `E`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.E.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.E.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_e(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `E`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.E.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.E.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_e_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `M`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.M.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.M.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.M.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_m(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `M`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.M.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.M.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.M.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_m_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YM`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_ym(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `YM`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.YM.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_ym_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `Y`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_y(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `Y`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html#method.with_alignment), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html#method.with_year_style), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.Y.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_y_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.format) for more information.
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline std::string format_iso(const icu4x::IsoDate& start_iso_date, const icu4x::IsoDate& end_iso_date) const;
  template<typename W>
  inline void format_iso_write(const icu4x::IsoDate& start_iso_date, const icu4x::IsoDate& end_iso_date, W& writeable_output) const;

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.4.0/icu/datetime/range/struct.DateRangeFormatter.html#method.format) for more information.
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline std::string format_same_calendar(const icu4x::Date& start_date, const icu4x::Date& end_date) const;
  template<typename W>
  inline void format_same_calendar_write(const icu4x::Date& start_date, const icu4x::Date& end_date, W& writeable_output) const;

    inline const icu4x::capi::DateRangeFormatter* AsFFI() const;
    inline icu4x::capi::DateRangeFormatter* AsFFI();
    inline static const icu4x::DateRangeFormatter* FromFFI(const icu4x::capi::DateRangeFormatter* ptr);
    inline static icu4x::DateRangeFormatter* FromFFI(icu4x::capi::DateRangeFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    DateRangeFormatter() = delete;
    DateRangeFormatter(const icu4x::DateRangeFormatter&) = delete;
    DateRangeFormatter(icu4x::DateRangeFormatter&&) noexcept = delete;
    DateRangeFormatter operator=(const icu4x::DateRangeFormatter&) = delete;
    DateRangeFormatter operator=(icu4x::DateRangeFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_DateRangeFormatter_D_HPP
