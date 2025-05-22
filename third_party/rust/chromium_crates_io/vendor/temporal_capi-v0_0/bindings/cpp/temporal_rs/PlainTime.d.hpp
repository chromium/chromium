#ifndef temporal_rs_PlainTime_D_HPP
#define temporal_rs_PlainTime_D_HPP

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
namespace capi { struct Duration; }
class Duration;
namespace capi { struct PlainTime; }
class PlainTime;
namespace capi { struct TimeDuration; }
class TimeDuration;
struct DifferenceSettings;
struct PartialTime;
struct TemporalError;
struct ToStringRoundingOptions;
class ArithmeticOverflow;
class RoundingMode;
class Unit;
}


namespace temporal_rs {
namespace capi {
    struct PlainTime;
} // namespace capi
} // namespace

namespace temporal_rs {
class PlainTime {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> create(uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> try_create(uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> from_partial(temporal_rs::PartialTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow);

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> with(temporal_rs::PartialTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline uint8_t hour() const;

  inline uint8_t minute() const;

  inline uint8_t second() const;

  inline uint16_t millisecond() const;

  inline uint16_t microsecond() const;

  inline uint16_t nanosecond() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> add(const temporal_rs::Duration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> add_time_duration(const temporal_rs::TimeDuration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> subtract_time_duration(const temporal_rs::TimeDuration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> until(const temporal_rs::PlainTime& other, temporal_rs::DifferenceSettings settings) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> since(const temporal_rs::PlainTime& other, temporal_rs::DifferenceSettings settings) const;

  inline bool equals(const temporal_rs::PlainTime& other) const;

  inline static int8_t compare(const temporal_rs::PlainTime& one, const temporal_rs::PlainTime& two);

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainTime>, temporal_rs::TemporalError> round(temporal_rs::Unit smallest_unit, std::optional<double> rounding_increment, std::optional<temporal_rs::RoundingMode> rounding_mode) const;

  inline diplomat::result<std::string, temporal_rs::TemporalError> to_ixdtf_string(temporal_rs::ToStringRoundingOptions options) const;

  inline const temporal_rs::capi::PlainTime* AsFFI() const;
  inline temporal_rs::capi::PlainTime* AsFFI();
  inline static const temporal_rs::PlainTime* FromFFI(const temporal_rs::capi::PlainTime* ptr);
  inline static temporal_rs::PlainTime* FromFFI(temporal_rs::capi::PlainTime* ptr);
  inline static void operator delete(void* ptr);
private:
  PlainTime() = delete;
  PlainTime(const temporal_rs::PlainTime&) = delete;
  PlainTime(temporal_rs::PlainTime&&) noexcept = delete;
  PlainTime operator=(const temporal_rs::PlainTime&) = delete;
  PlainTime operator=(temporal_rs::PlainTime&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_PlainTime_D_HPP
