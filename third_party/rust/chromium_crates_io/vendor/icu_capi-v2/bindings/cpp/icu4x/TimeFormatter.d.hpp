#ifndef ICU4X_TimeFormatter_D_HPP
#define ICU4X_TimeFormatter_D_HPP

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
namespace capi { struct TimeFormatter; }
class TimeFormatter;
class DateTimeAlignment;
class DateTimeFormatterLoadError;
class DateTimeLength;
class TimePrecision;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `NoCalendarFormatter`](https://docs.rs/icu/2.2.0/icu/datetime/type.NoCalendarFormatter.html) for more information.
 */
class TimeFormatter {
public:

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/type.NoCalendarFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `T`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeFormatter>, icu4x::DateTimeFormatterLoadError> create(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/datetime/type.NoCalendarFormatter.html#method.try_new) for more information.
   *
   * See the [Rust documentation for `T`](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html#method.with_time_precision), [2](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html#method.with_alignment), [3](https://docs.rs/icu/2.2.0/icu/datetime/fieldsets/struct.T.html#method.for_length)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/datetime/type.NoCalendarFormatter.html#method.format) for more information.
   */
  inline std::string format(const icu4x::Time& time) const;
  template<typename W>
  inline void format_write(const icu4x::Time& time, W& writeable_output) const;

    inline const icu4x::capi::TimeFormatter* AsFFI() const;
    inline icu4x::capi::TimeFormatter* AsFFI();
    inline static const icu4x::TimeFormatter* FromFFI(const icu4x::capi::TimeFormatter* ptr);
    inline static icu4x::TimeFormatter* FromFFI(icu4x::capi::TimeFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    TimeFormatter() = delete;
    TimeFormatter(const icu4x::TimeFormatter&) = delete;
    TimeFormatter(icu4x::TimeFormatter&&) noexcept = delete;
    TimeFormatter operator=(const icu4x::TimeFormatter&) = delete;
    TimeFormatter operator=(icu4x::TimeFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TimeFormatter_D_HPP
