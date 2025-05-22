#ifndef temporal_rs_Instant_D_HPP
#define temporal_rs_Instant_D_HPP

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
namespace capi { struct Instant; }
class Instant;
namespace capi { struct TimeDuration; }
class TimeDuration;
namespace capi { struct TimeZone; }
class TimeZone;
struct DifferenceSettings;
struct I128Nanoseconds;
struct RoundingOptions;
struct TemporalError;
struct ToStringRoundingOptions;
}


namespace temporal_rs {
namespace capi {
    struct Instant;
} // namespace capi
} // namespace

namespace temporal_rs {
class Instant {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> try_new(temporal_rs::I128Nanoseconds ns);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> from_epoch_milliseconds(int64_t epoch_milliseconds);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> add(const temporal_rs::Duration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> add_time_duration(const temporal_rs::TimeDuration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> subtract_time_duration(const temporal_rs::TimeDuration& duration) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> since(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> until(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> round(temporal_rs::RoundingOptions options) const;

  inline int64_t epoch_milliseconds() const;

  inline temporal_rs::I128Nanoseconds epoch_nanoseconds() const;

  inline diplomat::result<std::string, temporal_rs::TemporalError> to_ixdtf_string_with_compiled_data(const temporal_rs::TimeZone* zone, temporal_rs::ToStringRoundingOptions options) const;

  inline const temporal_rs::capi::Instant* AsFFI() const;
  inline temporal_rs::capi::Instant* AsFFI();
  inline static const temporal_rs::Instant* FromFFI(const temporal_rs::capi::Instant* ptr);
  inline static temporal_rs::Instant* FromFFI(temporal_rs::capi::Instant* ptr);
  inline static void operator delete(void* ptr);
private:
  Instant() = delete;
  Instant(const temporal_rs::Instant&) = delete;
  Instant(temporal_rs::Instant&&) noexcept = delete;
  Instant operator=(const temporal_rs::Instant&) = delete;
  Instant operator=(temporal_rs::Instant&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_Instant_D_HPP
