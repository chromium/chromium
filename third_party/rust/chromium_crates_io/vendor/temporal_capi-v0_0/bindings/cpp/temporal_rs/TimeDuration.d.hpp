#ifndef temporal_rs_TimeDuration_D_HPP
#define temporal_rs_TimeDuration_D_HPP

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
namespace capi { struct TimeDuration; }
class TimeDuration;
struct TemporalError;
class Sign;
}


namespace temporal_rs {
namespace capi {
    struct TimeDuration;
} // namespace capi
} // namespace

namespace temporal_rs {
class TimeDuration {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeDuration>, temporal_rs::TemporalError> new_(int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

  inline std::unique_ptr<temporal_rs::TimeDuration> abs() const;

  inline std::unique_ptr<temporal_rs::TimeDuration> negated() const;

  inline bool is_within_range() const;

  inline temporal_rs::Sign sign() const;

  inline const temporal_rs::capi::TimeDuration* AsFFI() const;
  inline temporal_rs::capi::TimeDuration* AsFFI();
  inline static const temporal_rs::TimeDuration* FromFFI(const temporal_rs::capi::TimeDuration* ptr);
  inline static temporal_rs::TimeDuration* FromFFI(temporal_rs::capi::TimeDuration* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeDuration() = delete;
  TimeDuration(const temporal_rs::TimeDuration&) = delete;
  TimeDuration(temporal_rs::TimeDuration&&) noexcept = delete;
  TimeDuration operator=(const temporal_rs::TimeDuration&) = delete;
  TimeDuration operator=(temporal_rs::TimeDuration&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_TimeDuration_D_HPP
