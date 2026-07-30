#ifndef ICU4X_TimeZone_D_HPP
#define ICU4X_TimeZone_D_HPP

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
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffset; }
class UtcOffset;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeZone;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TimeZone`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html) for more information.
 */
class TimeZone {
public:

  /**
   * The unknown time zone.
   *
   * See the [Rust documentation for `unknown`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZoneInfo.html#method.unknown) for more information.
   */
  inline static std::unique_ptr<icu4x::TimeZone> unknown();

  /**
   * Whether the time zone is the unknown zone.
   *
   * See the [Rust documentation for `is_unknown`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html#method.is_unknown) for more information.
   */
  inline bool is_unknown() const;

  /**
   * Construct a {@link TimeZone} from an IANA time zone ID.
   *
   * See the [Rust documentation for `from_iana_id`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html#method.from_iana_id) for more information.
   */
  inline static std::unique_ptr<icu4x::TimeZone> create_from_iana_id(std::string_view iana_id);

  /**
   * Construct a {@link TimeZone} from a Windows time zone ID and region.
   *
   * See the [Rust documentation for `from_windows_id`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html#method.from_windows_id) for more information.
   */
  inline static std::unique_ptr<icu4x::TimeZone> create_from_windows_id(std::string_view windows_id, std::string_view region);

  /**
   * Construct a {@link TimeZone} from the platform-specific ID.
   *
   * See the [Rust documentation for `from_system_id`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html#method.from_system_id) for more information.
   */
  inline static std::unique_ptr<icu4x::TimeZone> create_from_system_id(std::string_view id, std::string_view _region);

  /**
   * Creates a time zone from a BCP-47 string.
   *
   * Returns the unknown time zone if the string is not a valid BCP-47 subtag.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html)
   */
  inline static std::unique_ptr<icu4x::TimeZone> create_from_bcp47(std::string_view id);

  /**
   * See the [Rust documentation for `with_offset`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html#method.with_offset) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> with_offset(const icu4x::UtcOffset& offset) const;

  /**
   * See the [Rust documentation for `without_offset`](https://docs.rs/icu/2.2.0/icu/time/struct.TimeZone.html#method.without_offset) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> without_offset() const;

    inline const icu4x::capi::TimeZone* AsFFI() const;
    inline icu4x::capi::TimeZone* AsFFI();
    inline static const icu4x::TimeZone* FromFFI(const icu4x::capi::TimeZone* ptr);
    inline static icu4x::TimeZone* FromFFI(icu4x::capi::TimeZone* ptr);
    inline static void operator delete(void* ptr);
private:
    TimeZone() = delete;
    TimeZone(const icu4x::TimeZone&) = delete;
    TimeZone(icu4x::TimeZone&&) noexcept = delete;
    TimeZone operator=(const icu4x::TimeZone&) = delete;
    TimeZone operator=(icu4x::TimeZone&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TimeZone_D_HPP
