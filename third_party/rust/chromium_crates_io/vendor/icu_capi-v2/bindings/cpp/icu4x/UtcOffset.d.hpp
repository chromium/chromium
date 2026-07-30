#ifndef ICU4X_UtcOffset_D_HPP
#define ICU4X_UtcOffset_D_HPP

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
namespace capi { struct UtcOffset; }
class UtcOffset;
struct TimeZoneInvalidOffsetError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct UtcOffset;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `UtcOffset`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html) for more information.
 */
class UtcOffset {
public:

  /**
   * Creates an offset from seconds.
   *
   * Errors if the offset seconds are out of range.
   *
   * See the [Rust documentation for `try_from_seconds`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.try_from_seconds) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError> from_seconds(int32_t seconds);

  /**
   * Creates an offset from a string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.try_from_str) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html)
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError> from_string(std::string_view offset);

  /**
   * Returns the value as offset seconds.
   *
   * See the [Rust documentation for `offset`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.offset) for more information.
   *
   * See the [Rust documentation for `to_seconds`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.to_seconds) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html)
   */
  inline int32_t seconds() const;

  /**
   * Returns whether the offset is positive.
   *
   * See the [Rust documentation for `is_non_negative`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.is_non_negative) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html)
   */
  inline bool is_non_negative() const;

  /**
   * Returns whether the offset is zero.
   *
   * See the [Rust documentation for `is_zero`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.is_zero) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html)
   */
  inline bool is_zero() const;

  /**
   * Returns the hours part of the offset.
   *
   * See the [Rust documentation for `hours_part`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.hours_part) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html)
   */
  inline int32_t hours_part() const;

  /**
   * Returns the minutes part of the offset.
   *
   * See the [Rust documentation for `minutes_part`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.minutes_part) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html)
   */
  inline uint32_t minutes_part() const;

  /**
   * Returns the seconds part of the offset.
   *
   * See the [Rust documentation for `seconds_part`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html#method.seconds_part) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.UtcOffset.html)
   */
  inline uint32_t seconds_part() const;

    inline const icu4x::capi::UtcOffset* AsFFI() const;
    inline icu4x::capi::UtcOffset* AsFFI();
    inline static const icu4x::UtcOffset* FromFFI(const icu4x::capi::UtcOffset* ptr);
    inline static icu4x::UtcOffset* FromFFI(icu4x::capi::UtcOffset* ptr);
    inline static void operator delete(void* ptr);
private:
    UtcOffset() = delete;
    UtcOffset(const icu4x::UtcOffset&) = delete;
    UtcOffset(icu4x::UtcOffset&&) noexcept = delete;
    UtcOffset operator=(const icu4x::UtcOffset&) = delete;
    UtcOffset operator=(icu4x::UtcOffset&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_UtcOffset_D_HPP
