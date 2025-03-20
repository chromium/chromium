#ifndef temporal_rs_Duration_D_HPP
#define temporal_rs_Duration_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace temporal_rs {
namespace capi { struct DateDuration; }
class DateDuration;
namespace capi { struct Duration; }
class Duration;
namespace capi { struct TimeDuration; }
class TimeDuration;
struct PartialDuration;
struct TemporalError;
class Sign;
}


namespace temporal_rs {
namespace capi {
    struct Duration;
} // namespace capi
} // namespace

namespace temporal_rs {
class Duration {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> create(double years, double months, double weeks, double days, double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_day_and_time(double day, const temporal_rs::TimeDuration& time);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_partial_duration(temporal_rs::PartialDuration partial);

  inline bool is_time_within_range() const;

  inline const temporal_rs::TimeDuration& time() const;

  inline const temporal_rs::DateDuration& date() const;

  inline double years() const;

  inline double months() const;

  inline double weeks() const;

  inline double days() const;

  inline double hours() const;

  inline double minutes() const;

  inline double seconds() const;

  inline double milliseconds() const;

  inline double microseconds() const;

  inline double nanoseconds() const;

  inline temporal_rs::Sign sign() const;

  inline bool is_zero() const;

  inline std::unique_ptr<temporal_rs::Duration> abs() const;

  inline std::unique_ptr<temporal_rs::Duration> negated() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> add(const temporal_rs::Duration& other) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& other) const;

  inline const temporal_rs::capi::Duration* AsFFI() const;
  inline temporal_rs::capi::Duration* AsFFI();
  inline static const temporal_rs::Duration* FromFFI(const temporal_rs::capi::Duration* ptr);
  inline static temporal_rs::Duration* FromFFI(temporal_rs::capi::Duration* ptr);
  inline static void operator delete(void* ptr);
private:
  Duration() = delete;
  Duration(const temporal_rs::Duration&) = delete;
  Duration(temporal_rs::Duration&&) noexcept = delete;
  Duration operator=(const temporal_rs::Duration&) = delete;
  Duration operator=(temporal_rs::Duration&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_Duration_D_HPP
