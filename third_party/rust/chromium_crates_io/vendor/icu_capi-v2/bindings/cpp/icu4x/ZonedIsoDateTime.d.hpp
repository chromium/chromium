#ifndef ICU4X_ZonedIsoDateTime_D_HPP
#define ICU4X_ZonedIsoDateTime_D_HPP

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
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffset; }
class UtcOffset;
namespace capi { struct VariantOffsetsCalculator; }
class VariantOffsetsCalculator;
struct ZonedIsoDateTime;
class Rfc9557ParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ZonedIsoDateTime {
      icu4x::capi::IsoDate* date;
      icu4x::capi::Time* time;
      icu4x::capi::TimeZoneInfo* zone;
    };

    typedef struct ZonedIsoDateTime_option {union { ZonedIsoDateTime ok; }; bool is_ok; } ZonedIsoDateTime_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * An ICU4X `ZonedDateTime` object capable of containing a ISO-8601 date, time, and zone.
 *
 * See the [Rust documentation for `ZonedDateTime`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html) for more information.
 */
struct ZonedIsoDateTime {
    std::unique_ptr<icu4x::IsoDate> date;
    std::unique_ptr<icu4x::Time> time;
    std::unique_ptr<icu4x::TimeZoneInfo> zone;

  /**
   * Creates a new {@link ZonedIsoDateTime} from an IXDTF string.
   *
   * See the [Rust documentation for `try_strict_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.try_strict_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError> strict_from_string(std::string_view v, const icu4x::IanaParser& iana_parser);

  /**
   * Creates a new {@link ZonedIsoDateTime} from an IXDTF string.
   *
   * See the [Rust documentation for `try_full_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.try_full_from_str) for more information.
   *
   * \deprecated use strict_from_string
   */
  [[deprecated("use strict_from_string")]]
  inline static icu4x::diplomat::result<icu4x::ZonedIsoDateTime, icu4x::Rfc9557ParseError> full_from_string(std::string_view v, const icu4x::IanaParser& iana_parser, const icu4x::VariantOffsetsCalculator& _offset_calculator);

  /**
   * Creates a new {@link ZonedIsoDateTime} from milliseconds since epoch (timestamp) and a UTC offset.
   *
   * Note: {@link ZonedIsoDateTime}s created with this constructor can only be formatted using localized offset zone styles.
   *
   * See the [Rust documentation for `from_epoch_milliseconds_and_utc_offset`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.from_epoch_milliseconds_and_utc_offset) for more information.
   */
  inline static icu4x::ZonedIsoDateTime from_epoch_milliseconds_and_utc_offset(int64_t epoch_milliseconds, const icu4x::UtcOffset& utc_offset);

    inline icu4x::capi::ZonedIsoDateTime AsFFI() const;
    inline static icu4x::ZonedIsoDateTime FromFFI(icu4x::capi::ZonedIsoDateTime c_struct);
};

} // namespace
#endif // ICU4X_ZonedIsoDateTime_D_HPP
