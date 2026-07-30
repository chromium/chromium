#ifndef ICU4X_Time_D_HPP
#define ICU4X_Time_D_HPP

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
namespace capi { struct Time; }
class Time;
class CalendarError;
class Rfc9557ParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct Time;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Time object representing a time in terms of hour, minute, second, nanosecond
 *
 * See the [Rust documentation for `Time`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html) for more information.
 */
class Time {
public:

  /**
   * Creates a new {@link Time} given field values
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> create(uint8_t hour, uint8_t minute, uint8_t second, uint32_t subsecond);

  /**
   * Creates a new {@link Time} from an IXDTF string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::Rfc9557ParseError> from_string(std::string_view v);

  /**
   * Creates a new {@link Time} representing the start of the day (00:00:00.000).
   *
   * See the [Rust documentation for `start_of_day`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#method.start_of_day) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> start_of_day();

  /**
   * Creates a new {@link Time} representing noon (12:00:00.000).
   *
   * See the [Rust documentation for `noon`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#method.noon) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> noon();

  /**
   * Returns the hour in this time
   *
   * See the [Rust documentation for `hour`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#structfield.hour) for more information.
   */
  inline uint8_t hour() const;

  /**
   * Returns the minute in this time
   *
   * See the [Rust documentation for `minute`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#structfield.minute) for more information.
   */
  inline uint8_t minute() const;

  /**
   * Returns the second in this time
   *
   * See the [Rust documentation for `second`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#structfield.second) for more information.
   */
  inline uint8_t second() const;

  /**
   * Returns the subsecond in this time as nanoseconds
   *
   * See the [Rust documentation for `subsecond`](https://docs.rs/icu/2.2.0/icu/time/struct.Time.html#structfield.subsecond) for more information.
   */
  inline uint32_t subsecond() const;

    inline const icu4x::capi::Time* AsFFI() const;
    inline icu4x::capi::Time* AsFFI();
    inline static const icu4x::Time* FromFFI(const icu4x::capi::Time* ptr);
    inline static icu4x::Time* FromFFI(icu4x::capi::Time* ptr);
    inline static void operator delete(void* ptr);
private:
    Time() = delete;
    Time(const icu4x::Time&) = delete;
    Time(icu4x::Time&&) noexcept = delete;
    Time operator=(const icu4x::Time&) = delete;
    Time operator=(icu4x::Time&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_Time_D_HPP
