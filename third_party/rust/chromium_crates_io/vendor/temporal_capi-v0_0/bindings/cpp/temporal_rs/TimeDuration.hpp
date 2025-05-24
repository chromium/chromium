#ifndef temporal_rs_TimeDuration_HPP
#define temporal_rs_TimeDuration_HPP

#include "TimeDuration.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "Sign.hpp"
#include "TemporalError.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_TimeDuration_new_result {union {temporal_rs::capi::TimeDuration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_TimeDuration_new_result;
    temporal_rs_TimeDuration_new_result temporal_rs_TimeDuration_new(int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

    temporal_rs::capi::TimeDuration* temporal_rs_TimeDuration_abs(const temporal_rs::capi::TimeDuration* self);

    temporal_rs::capi::TimeDuration* temporal_rs_TimeDuration_negated(const temporal_rs::capi::TimeDuration* self);

    bool temporal_rs_TimeDuration_is_within_range(const temporal_rs::capi::TimeDuration* self);

    temporal_rs::capi::Sign temporal_rs_TimeDuration_sign(const temporal_rs::capi::TimeDuration* self);

    void temporal_rs_TimeDuration_destroy(TimeDuration* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::TimeDuration>, temporal_rs::TemporalError> temporal_rs::TimeDuration::new_(int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds) {
  auto result = temporal_rs::capi::temporal_rs_TimeDuration_new(hours,
    minutes,
    seconds,
    milliseconds,
    microseconds,
    nanoseconds);
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::TimeDuration>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::TimeDuration>>(std::unique_ptr<temporal_rs::TimeDuration>(temporal_rs::TimeDuration::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::TimeDuration>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::TimeDuration> temporal_rs::TimeDuration::abs() const {
  auto result = temporal_rs::capi::temporal_rs_TimeDuration_abs(this->AsFFI());
  return std::unique_ptr<temporal_rs::TimeDuration>(temporal_rs::TimeDuration::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::TimeDuration> temporal_rs::TimeDuration::negated() const {
  auto result = temporal_rs::capi::temporal_rs_TimeDuration_negated(this->AsFFI());
  return std::unique_ptr<temporal_rs::TimeDuration>(temporal_rs::TimeDuration::FromFFI(result));
}

inline bool temporal_rs::TimeDuration::is_within_range() const {
  auto result = temporal_rs::capi::temporal_rs_TimeDuration_is_within_range(this->AsFFI());
  return result;
}

inline temporal_rs::Sign temporal_rs::TimeDuration::sign() const {
  auto result = temporal_rs::capi::temporal_rs_TimeDuration_sign(this->AsFFI());
  return temporal_rs::Sign::FromFFI(result);
}

inline const temporal_rs::capi::TimeDuration* temporal_rs::TimeDuration::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::TimeDuration*>(this);
}

inline temporal_rs::capi::TimeDuration* temporal_rs::TimeDuration::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::TimeDuration*>(this);
}

inline const temporal_rs::TimeDuration* temporal_rs::TimeDuration::FromFFI(const temporal_rs::capi::TimeDuration* ptr) {
  return reinterpret_cast<const temporal_rs::TimeDuration*>(ptr);
}

inline temporal_rs::TimeDuration* temporal_rs::TimeDuration::FromFFI(temporal_rs::capi::TimeDuration* ptr) {
  return reinterpret_cast<temporal_rs::TimeDuration*>(ptr);
}

inline void temporal_rs::TimeDuration::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_TimeDuration_destroy(reinterpret_cast<temporal_rs::capi::TimeDuration*>(ptr));
}


#endif // temporal_rs_TimeDuration_HPP
