#ifndef icu4x_TimeZoneInfo_D_HPP
#define icu4x_TimeZoneInfo_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffset; }
class UtcOffset;
namespace capi { struct UtcOffsetCalculator; }
class UtcOffsetCalculator;
struct IsoDateTime;
class TimeZoneVariant;
}


namespace icu4x {
namespace capi {
    struct TimeZoneInfo;
} // namespace capi
} // namespace

namespace icu4x {
class TimeZoneInfo {
public:

  inline static std::unique_ptr<icu4x::TimeZoneInfo> utc();

  inline static std::unique_ptr<icu4x::TimeZoneInfo> from_parts(const icu4x::TimeZone& time_zone_id, const icu4x::UtcOffset* offset, std::optional<icu4x::TimeZoneVariant> zone_variant);

  inline std::unique_ptr<icu4x::TimeZone> time_zone_id() const;

  inline std::unique_ptr<icu4x::TimeZoneInfo> at_time(const icu4x::IsoDate& date, const icu4x::Time& time) const;

  inline std::optional<icu4x::IsoDateTime> local_time() const;

  inline std::unique_ptr<icu4x::TimeZoneInfo> with_zone_variant(icu4x::TimeZoneVariant time_zone_variant) const;

  inline std::optional<std::monostate> infer_zone_variant(const icu4x::UtcOffsetCalculator& offset_calculator);

  inline std::optional<icu4x::TimeZoneVariant> zone_variant() const;

  inline const icu4x::capi::TimeZoneInfo* AsFFI() const;
  inline icu4x::capi::TimeZoneInfo* AsFFI();
  inline static const icu4x::TimeZoneInfo* FromFFI(const icu4x::capi::TimeZoneInfo* ptr);
  inline static icu4x::TimeZoneInfo* FromFFI(icu4x::capi::TimeZoneInfo* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZoneInfo() = delete;
  TimeZoneInfo(const icu4x::TimeZoneInfo&) = delete;
  TimeZoneInfo(icu4x::TimeZoneInfo&&) noexcept = delete;
  TimeZoneInfo operator=(const icu4x::TimeZoneInfo&) = delete;
  TimeZoneInfo operator=(icu4x::TimeZoneInfo&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeZoneInfo_D_HPP
