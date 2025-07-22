#ifndef temporal_rs_ZonedDateTime_D_HPP
#define temporal_rs_ZonedDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"

namespace temporal_rs {
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Duration; }
class Duration;
namespace capi { struct Instant; }
class Instant;
namespace capi { struct OwnedPartialZonedDateTime; }
class OwnedPartialZonedDateTime;
namespace capi { struct PlainDate; }
class PlainDate;
namespace capi { struct PlainDateTime; }
class PlainDateTime;
namespace capi { struct PlainTime; }
class PlainTime;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct ZonedDateTime; }
class ZonedDateTime;
struct DifferenceSettings;
struct I128Nanoseconds;
struct PartialZonedDateTime;
struct RoundingOptions;
struct TemporalError;
struct ToStringRoundingOptions;
class AnyCalendarKind;
class ArithmeticOverflow;
class Disambiguation;
class DisplayCalendar;
class DisplayOffset;
class DisplayTimeZone;
class OffsetDisambiguation;
class TransitionDirection;
}


namespace temporal_rs {
namespace capi {
    struct ZonedDateTime;
} // namespace capi
} // namespace

namespace temporal_rs {
class ZonedDateTime {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> try_new(temporal_rs::I128Nanoseconds nanosecond, temporal_rs::AnyCalendarKind calendar, const temporal_rs::TimeZone& time_zone);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> from_partial(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> from_owned_partial(const temporal_rs::OwnedPartialZonedDateTime& partial, std::optional<temporal_rs::ArithmeticOverflow> overflow, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> from_utf8(std::string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> from_utf16(std::u16string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation);

  inline int64_t epoch_milliseconds() const;

  inline static diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> from_epoch_milliseconds(int64_t ms, const temporal_rs::TimeZone& tz);

  inline temporal_rs::I128Nanoseconds epoch_nanoseconds() const;

  inline diplomat::result<int64_t, temporal_rs::TemporalError> offset_nanoseconds() const;

  inline std::unique_ptr<temporal_rs::Instant> to_instant() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> with(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> with_timezone(const temporal_rs::TimeZone& zone) const;

  inline const temporal_rs::TimeZone& timezone() const;

  inline int8_t compare_instant(const temporal_rs::ZonedDateTime& other) const;

  inline bool equals(const temporal_rs::ZonedDateTime& other) const;

  inline diplomat::result<std::string, temporal_rs::TemporalError> offset() const;
  template<typename W>
  inline diplomat::result<std::monostate, temporal_rs::TemporalError> offset_write(W& writeable_output) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> start_of_day() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> get_time_zone_transition(temporal_rs::TransitionDirection direction) const;

  inline diplomat::result<uint8_t, temporal_rs::TemporalError> hours_in_day() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> to_plain_datetime() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> to_plain_date() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> to_plain_time() const;

  inline diplomat::result<std::string, temporal_rs::TemporalError> to_ixdtf_string(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options) const;
  template<typename W>
  inline diplomat::result<std::monostate, temporal_rs::TemporalError> to_ixdtf_string_write(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options, W& writeable_output) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> with_calendar(temporal_rs::AnyCalendarKind calendar) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> with_plain_time(const temporal_rs::PlainTime* time) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> add(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> until(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> since(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> round(temporal_rs::RoundingOptions options) const;

  inline uint8_t hour() const;

  inline uint8_t minute() const;

  inline uint8_t second() const;

  inline uint16_t millisecond() const;

  inline uint16_t microsecond() const;

  inline uint16_t nanosecond() const;

  inline const temporal_rs::Calendar& calendar() const;

  inline int32_t year() const;

  inline uint8_t month() const;

  inline std::string month_code() const;
  template<typename W>
  inline void month_code_write(W& writeable_output) const;

  inline uint8_t day() const;

  inline diplomat::result<uint16_t, temporal_rs::TemporalError> day_of_week() const;

  inline uint16_t day_of_year() const;

  inline std::optional<uint8_t> week_of_year() const;

  inline std::optional<int32_t> year_of_week() const;

  inline diplomat::result<uint16_t, temporal_rs::TemporalError> days_in_week() const;

  inline uint16_t days_in_month() const;

  inline uint16_t days_in_year() const;

  inline uint16_t months_in_year() const;

  inline bool in_leap_year() const;

  inline std::string era() const;
  template<typename W>
  inline void era_write(W& writeable_output) const;

  inline std::optional<int32_t> era_year() const;

  inline const temporal_rs::capi::ZonedDateTime* AsFFI() const;
  inline temporal_rs::capi::ZonedDateTime* AsFFI();
  inline static const temporal_rs::ZonedDateTime* FromFFI(const temporal_rs::capi::ZonedDateTime* ptr);
  inline static temporal_rs::ZonedDateTime* FromFFI(temporal_rs::capi::ZonedDateTime* ptr);
  inline static void operator delete(void* ptr);
private:
  ZonedDateTime() = delete;
  ZonedDateTime(const temporal_rs::ZonedDateTime&) = delete;
  ZonedDateTime(temporal_rs::ZonedDateTime&&) noexcept = delete;
  ZonedDateTime operator=(const temporal_rs::ZonedDateTime&) = delete;
  ZonedDateTime operator=(temporal_rs::ZonedDateTime&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_ZonedDateTime_D_HPP
