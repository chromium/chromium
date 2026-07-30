#ifndef ICU4X_DateTime_D_HPP
#define ICU4X_DateTime_D_HPP

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
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Date; }
class Date;
namespace capi { struct Time; }
class Time;
struct DateTime;
class Rfc9557ParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateTime {
      icu4x::capi::Date* date;
      icu4x::capi::Time* time;
    };

    typedef struct DateTime_option {union { DateTime ok; }; bool is_ok; } DateTime_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * An ICU4X `DateTime` object capable of containing a date and time for any calendar.
 *
 * See the [Rust documentation for `DateTime`](https://docs.rs/icu/2.2.0/icu/time/struct.DateTime.html) for more information.
 */
struct DateTime {
    std::unique_ptr<icu4x::Date> date;
    std::unique_ptr<icu4x::Time> time;

  /**
   * Creates a new {@link DateTime} from an IXDTF string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.DateTime.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::DateTime, icu4x::Rfc9557ParseError> from_string(std::string_view v, const icu4x::Calendar& calendar);

    inline icu4x::capi::DateTime AsFFI() const;
    inline static icu4x::DateTime FromFFI(icu4x::capi::DateTime c_struct);
};

} // namespace
#endif // ICU4X_DateTime_D_HPP
