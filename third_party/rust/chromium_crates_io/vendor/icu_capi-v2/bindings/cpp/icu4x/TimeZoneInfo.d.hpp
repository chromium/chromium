#ifndef ICU4X_TimeZoneInfo_D_HPP
#define ICU4X_TimeZoneInfo_D_HPP

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
namespace capi { struct Date; }
class Date;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffset; }
class UtcOffset;
namespace capi { struct VariantOffsetsCalculator; }
class VariantOffsetsCalculator;
struct IsoDateTime;
class TimeZoneVariant;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeZoneInfo;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TimeZoneInfo`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html) for more information.
 */
class TimeZoneInfo {
public:

  /**
   * Creates a time zone for UTC (Coordinated Universal Time).
   *
   * See the [Rust documentation for `utc`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.utc) for more information.
   */
  inline static std::unique_ptr<icu4x::TimeZoneInfo> utc();

  /**
   * Creates a time zone info from parts.
   *
   * `variant` is ignored.
   */
  inline static std::unique_ptr<icu4x::TimeZoneInfo> from_parts(const icu4x::TimeZone& id, const icu4x::UtcOffset* offset, std::optional<icu4x::TimeZoneVariant> _variant);

  /**
   * See the [Rust documentation for `id`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.id) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZone> id() const;

  /**
   * Sets the datetime at which to interpret the time zone
   * for display name lookup.
   *
   * Notes:
   *
   * - If not set, the formatting datetime is used if possible.
   * - If the offset is not set, the datetime is interpreted as UTC.
   * - The constraints are the same as with `ZoneNameTimestamp` in Rust.
   * - Set to year 1000 or 9999 for a reference far in the past or future.
   *
   * See the [Rust documentation for `at_date_time`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.at_date_time) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.ZoneNameTimestamp.html)
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> at_date_time_iso(const icu4x::IsoDate& date, const icu4x::Time& time) const;

  /**
   * Sets the datetime at which to interpret the time zone
   * for display name lookup.
   *
   * Notes:
   *
   * - If not set, the formatting datetime is used if possible.
   * - If the offset is not set, the datetime is interpreted as UTC.
   * - The constraints are the same as with `ZoneNameTimestamp` in Rust.
   * - Set to year 1000 or 9999 for a reference far in the past or future.
   *
   * See the [Rust documentation for `at_date_time`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.at_date_time) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.ZoneNameTimestamp.html)
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> at_date_time(const icu4x::Date& date, const icu4x::Time& time) const;

  /**
   * Sets the timestamp, in milliseconds since Unix epoch, at which to interpret the time zone
   * for display name lookup.
   *
   * Notes:
   *
   * - If not set, the formatting datetime is used if possible.
   * - The constraints are the same as with `ZoneNameTimestamp` in Rust.
   *
   * See the [Rust documentation for `with_zone_name_timestamp`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.with_zone_name_timestamp) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/struct.ZoneNameTimestamp.html#method.from_epoch_seconds)
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> at_timestamp(int64_t timestamp) const;

  /**
   * Returns the `DateTime` for the UTC zone name reference time
   *
   * See the [Rust documentation for `zone_name_timestamp`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.zone_name_timestamp) for more information.
   */
  inline std::optional<icu4x::IsoDateTime> zone_name_date_time() const;

  /**
   * See the [Rust documentation for `with_variant`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.with_variant) for more information.
   *
   * \deprecated returns unmodified copy
   */
  [[deprecated("returns unmodified copy")]]
  inline std::unique_ptr<icu4x::TimeZoneInfo> with_variant(icu4x::TimeZoneVariant _time_variant) const;

  /**
   * See the [Rust documentation for `offset`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.offset) for more information.
   */
  inline std::unique_ptr<icu4x::UtcOffset> offset() const;

  /**
   * See the [Rust documentation for `infer_variant`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.infer_variant) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/zone/enum.TimeZoneVariant.html)
   *
   * \deprecated does nothing
   */
  [[deprecated("does nothing")]]
  inline std::optional<std::monostate> infer_variant(const icu4x::VariantOffsetsCalculator& _offset_calculator) const;

  /**
   * See the [Rust documentation for `variant`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.variant) for more information.
   *
   * \deprecated always returns null
   */
  [[deprecated("always returns null")]]
  inline std::optional<icu4x::TimeZoneVariant> variant() const;

    inline const icu4x::capi::TimeZoneInfo* AsFFI() const;
    inline icu4x::capi::TimeZoneInfo* AsFFI();
    inline static const icu4x::TimeZoneInfo* FromFFI(const icu4x::capi::TimeZoneInfo* ptr);
    inline static icu4x::TimeZoneInfo* FromFFI(icu4x::capi::TimeZoneInfo* ptr);
    inline static void operator delete(void* ptr);
private:
    TimeZoneInfo() = delete;
    TimeZoneInfo(const icu4x::TimeZoneInfo&) = delete;
    TimeZoneInfo(icu4x::TimeZoneInfo&&) noexcept = delete;
    TimeZoneInfo operator=(const icu4x::TimeZoneInfo&) = delete;
    TimeZoneInfo operator=(icu4x::TimeZoneInfo&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TimeZoneInfo_D_HPP
