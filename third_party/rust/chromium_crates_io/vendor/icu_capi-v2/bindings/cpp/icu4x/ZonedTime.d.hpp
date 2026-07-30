#ifndef ICU4X_ZonedTime_D_HPP
#define ICU4X_ZonedTime_D_HPP

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
namespace capi { struct IanaParser; }
class IanaParser;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
struct ZonedTime;
class Rfc9557ParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ZonedTime {
      icu4x::capi::Time* time;
      icu4x::capi::TimeZoneInfo* zone;
    };

    typedef struct ZonedTime_option {union { ZonedTime ok; }; bool is_ok; } ZonedTime_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * An ICU4X `ZonedTime` object capable of containing a ISO-8601 time, and zone.
 *
 * See the [Rust documentation for `ZonedTime`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedTime.html) for more information.
 */
struct ZonedTime {
    std::unique_ptr<icu4x::Time> time;
    std::unique_ptr<icu4x::TimeZoneInfo> zone;

  /**
   * Creates a new {@link ZonedTime} from an IXDTF string.
   *
   * See the [Rust documentation for `try_strict_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedTime.html#method.try_strict_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> strict_from_string(std::string_view v, const icu4x::IanaParser& iana_parser);

  /**
   * Creates a new {@link ZonedTime} from a location-only IXDTF string.
   *
   * See the [Rust documentation for `try_location_only_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedTime.html#method.try_location_only_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> location_only_from_string(std::string_view v, const icu4x::IanaParser& iana_parser);

  /**
   * Creates a new {@link ZonedTime} from an offset-only IXDTF string.
   *
   * See the [Rust documentation for `try_offset_only_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedTime.html#method.try_offset_only_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> offset_only_from_string(std::string_view v);

  /**
   * Creates a new {@link ZonedTime} from an IXDTF string, without requiring the offset.
   *
   * See the [Rust documentation for `try_lenient_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedTime.html#method.try_lenient_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedTime, icu4x::Rfc9557ParseError> lenient_from_string(std::string_view v, const icu4x::IanaParser& iana_parser);

    inline icu4x::capi::ZonedTime AsFFI() const;
    inline static icu4x::ZonedTime FromFFI(icu4x::capi::ZonedTime c_struct);
};

} // namespace
#endif // ICU4X_ZonedTime_D_HPP
