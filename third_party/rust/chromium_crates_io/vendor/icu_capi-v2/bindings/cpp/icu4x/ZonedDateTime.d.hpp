#ifndef ICU4X_ZonedDateTime_D_HPP
#define ICU4X_ZonedDateTime_D_HPP

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
namespace capi { struct IanaParser; }
class IanaParser;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct VariantOffsetsCalculator; }
class VariantOffsetsCalculator;
struct ZonedDateTime;
class Rfc9557ParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ZonedDateTime {
      icu4x::capi::Date* date;
      icu4x::capi::Time* time;
      icu4x::capi::TimeZoneInfo* zone;
    };

    typedef struct ZonedDateTime_option {union { ZonedDateTime ok; }; bool is_ok; } ZonedDateTime_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * An ICU4X `DateTime` object capable of containing a date, time, and zone for any calendar.
 *
 * See the [Rust documentation for `ZonedDateTime`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html) for more information.
 */
struct ZonedDateTime {
    std::unique_ptr<icu4x::Date> date;
    std::unique_ptr<icu4x::Time> time;
    std::unique_ptr<icu4x::TimeZoneInfo> zone;

  /**
   * Creates a new {@link ZonedIsoDateTime} from an IXDTF string.
   *
   * See the [Rust documentation for `try_strict_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.try_strict_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedDateTime, icu4x::Rfc9557ParseError> strict_from_string(std::string_view v, const icu4x::Calendar& calendar, const icu4x::IanaParser& iana_parser);

  /**
   * Creates a new {@link ZonedDateTime} from an IXDTF string.
   *
   * See the [Rust documentation for `try_full_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.try_full_from_str) for more information.
   *
   * \deprecated use strict_from_string
   */
  [[deprecated("use strict_from_string")]]
  inline static icu4x::diplomat::result<icu4x::ZonedDateTime, icu4x::Rfc9557ParseError> full_from_string(std::string_view v, const icu4x::Calendar& calendar, const icu4x::IanaParser& iana_parser, const icu4x::VariantOffsetsCalculator& _offset_calculator);

  /**
   * Creates a new {@link ZonedDateTime} from a location-only IXDTF string.
   *
   * See the [Rust documentation for `try_location_only_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.try_location_only_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedDateTime, icu4x::Rfc9557ParseError> location_only_from_string(std::string_view v, const icu4x::Calendar& calendar, const icu4x::IanaParser& iana_parser);

  /**
   * Creates a new {@link ZonedDateTime} from an offset-only IXDTF string.
   *
   * See the [Rust documentation for `try_offset_only_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.try_offset_only_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedDateTime, icu4x::Rfc9557ParseError> offset_only_from_string(std::string_view v, const icu4x::Calendar& calendar);

  /**
   * Creates a new {@link ZonedDateTime} from an IXDTF string, without requiring the offset.
   *
   * See the [Rust documentation for `try_lenient_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.ZonedDateTime.html#method.try_lenient_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::ZonedDateTime, icu4x::Rfc9557ParseError> lenient_from_string(std::string_view v, const icu4x::Calendar& calendar, const icu4x::IanaParser& iana_parser);

    inline icu4x::capi::ZonedDateTime AsFFI() const;
    inline static icu4x::ZonedDateTime FromFFI(icu4x::capi::ZonedDateTime c_struct);
};

} // namespace
#endif // ICU4X_ZonedDateTime_D_HPP
