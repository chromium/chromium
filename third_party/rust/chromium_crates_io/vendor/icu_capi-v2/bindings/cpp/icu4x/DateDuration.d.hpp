#ifndef ICU4X_DateDuration_D_HPP
#define ICU4X_DateDuration_D_HPP

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
struct DateDuration;
class DateDurationParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateDuration {
      bool is_negative;
      uint32_t years;
      uint32_t months;
      uint32_t weeks;
      uint32_t days;
    };

    typedef struct DateDuration_option {union { DateDuration ok; }; bool is_ok; } DateDuration_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `DateDuration`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.DateDuration.html) for more information.
 */
struct DateDuration {
    bool is_negative;
    uint32_t years;
    uint32_t months;
    uint32_t weeks;
    uint32_t days;

  /**
   * Creates a new {@link DateDuration} from an ISO 8601 string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.DateDuration.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::DateDuration, icu4x::DateDurationParseError> from_string(std::string_view v);

  /**
   * Returns a new {@link DateDuration} representing a number of years.
   *
   * See the [Rust documentation for `for_years`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.DateDuration.html#method.for_years) for more information.
   */
  inline static icu4x::DateDuration for_years(int32_t years);

  /**
   * Returns a new {@link DateDuration} representing a number of months.
   *
   * See the [Rust documentation for `for_months`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.DateDuration.html#method.for_months) for more information.
   */
  inline static icu4x::DateDuration for_months(int32_t months);

  /**
   * Returns a new {@link DateDuration} representing a number of weeks.
   *
   * See the [Rust documentation for `for_weeks`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.DateDuration.html#method.for_weeks) for more information.
   */
  inline static icu4x::DateDuration for_weeks(int32_t weeks);

  /**
   * Returns a new {@link DateDuration} representing a number of days.
   *
   * See the [Rust documentation for `for_days`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.DateDuration.html#method.for_days) for more information.
   */
  inline static icu4x::DateDuration for_days(int32_t days);

    inline icu4x::capi::DateDuration AsFFI() const;
    inline static icu4x::DateDuration FromFFI(icu4x::capi::DateDuration c_struct);
};

} // namespace
#endif // ICU4X_DateDuration_D_HPP
