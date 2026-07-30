#ifndef ICU4X_TimeZoneVariant_D_HPP
#define ICU4X_TimeZoneVariant_D_HPP

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
class TimeZoneVariant;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum TimeZoneVariant {
      TimeZoneVariant_Standard = 0,
      TimeZoneVariant_Daylight = 1,
    };

    typedef struct TimeZoneVariant_option {union { TimeZoneVariant ok; }; bool is_ok; } TimeZoneVariant_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TimeZoneVariant`](https://docs.rs/icu/2.2.0/icu/time/zone/enum.TimeZoneVariant.html) for more information.
 *
 * \deprecated type not needed anymore
 */
class [[deprecated("type not needed anymore")]] TimeZoneVariant {
public:
    enum Value {
        Standard = 0,
        Daylight = 1,
    };

    TimeZoneVariant(): value(Value::Standard) {}

    // Implicit conversions between enum and ::Value
    constexpr TimeZoneVariant(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `from_rearguard_isdst`](https://docs.rs/icu/2.2.0/icu/time/zone/enum.TimeZoneVariant.html#method.from_rearguard_isdst) for more information.
   *
   * See the [Rust documentation for `with_variant`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.with_variant) for more information.
   *
   * \deprecated type not needed anymore
   */
  [[deprecated("type not needed anymore")]]
  inline static icu4x::TimeZoneVariant from_rearguard_isdst(bool isdst);

    inline icu4x::capi::TimeZoneVariant AsFFI() const;
    inline static icu4x::TimeZoneVariant FromFFI(icu4x::capi::TimeZoneVariant c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_TimeZoneVariant_D_HPP
