#ifndef ICU4X_TimeRangeFormatter_D_HPP
#define ICU4X_TimeRangeFormatter_D_HPP

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
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeRangeFormatter; }
class TimeRangeFormatter;
class DateTimeAlignment;
class DateTimeFormatterLoadError;
class DateTimeLength;
class TimePrecision;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeRangeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `NoCalendarRangeFormatter`](https://docs.rs/icu/2.4.0/icu/datetime/range/type.NoCalendarRangeFormatter.html) for more information.
 *
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 */
class TimeRangeFormatter {
public:

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/type.NoCalendarRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `T`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError> create(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.4.0/icu/datetime/range/type.NoCalendarRangeFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `T`](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html#method.with_time_precision), [2](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html#method.with_alignment), [3](https://docs.rs/icu/2.4.0/icu/datetime/fieldsets/struct.T.html#method.for_length)
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.4.0/icu/datetime/range/type.NoCalendarRangeFormatter.html#method.format) for more information.
   *
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   */
  inline std::string format(const icu4x::Time& start_time, const icu4x::Time& end_time) const;
  template<typename W>
  inline void format_write(const icu4x::Time& start_time, const icu4x::Time& end_time, W& writeable_output) const;

    inline const icu4x::capi::TimeRangeFormatter* AsFFI() const;
    inline icu4x::capi::TimeRangeFormatter* AsFFI();
    inline static const icu4x::TimeRangeFormatter* FromFFI(const icu4x::capi::TimeRangeFormatter* ptr);
    inline static icu4x::TimeRangeFormatter* FromFFI(icu4x::capi::TimeRangeFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    TimeRangeFormatter() = delete;
    TimeRangeFormatter(const icu4x::TimeRangeFormatter&) = delete;
    TimeRangeFormatter(icu4x::TimeRangeFormatter&&) noexcept = delete;
    TimeRangeFormatter operator=(const icu4x::TimeRangeFormatter&) = delete;
    TimeRangeFormatter operator=(icu4x::TimeRangeFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TimeRangeFormatter_D_HPP
