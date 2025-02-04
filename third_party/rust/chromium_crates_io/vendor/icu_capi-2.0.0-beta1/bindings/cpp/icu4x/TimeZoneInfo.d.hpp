#ifndef icu4x_TimeZoneInfo_D_HPP
#define icu4x_TimeZoneInfo_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct IsoDateTime; }
class IsoDateTime;
namespace capi { struct TimeZoneIdMapper; }
class TimeZoneIdMapper;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
struct TimeZoneInvalidOffsetError;
}


namespace icu4x {
namespace capi {
    struct TimeZoneInfo;
} // namespace capi
} // namespace

namespace icu4x {
class TimeZoneInfo {
public:

  inline static std::unique_ptr<icu4x::TimeZoneInfo> unknown();

  inline static std::unique_ptr<icu4x::TimeZoneInfo> utc();

  inline static std::unique_ptr<icu4x::TimeZoneInfo> from_parts(std::string_view bcp47_id, int32_t offset_seconds, bool dst);

  inline diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError> try_set_offset_seconds(int32_t offset_seconds);

  inline void set_offset_eighths_of_hour(int8_t offset_eighths_of_hour);

  inline diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError> try_set_offset_str(std::string_view offset);

  inline std::optional<int8_t> offset_eighths_of_hour() const;

  inline void clear_offset();

  inline std::optional<int32_t> offset_seconds() const;

  inline std::optional<bool> is_offset_non_negative() const;

  inline std::optional<bool> is_offset_zero() const;

  inline std::optional<int32_t> offset_hours_part() const;

  inline std::optional<uint32_t> offset_minutes_part() const;

  inline std::optional<uint32_t> offset_seconds_part() const;

  inline void set_time_zone_id(std::string_view id);

  inline void set_iana_time_zone_id(const icu4x::TimeZoneIdMapper& mapper, std::string_view id);

  inline std::string time_zone_id() const;

  inline void clear_zone_variant();

  inline void set_standard_time();

  inline void set_daylight_time();

  inline std::optional<bool> is_standard_time() const;

  inline std::optional<bool> is_daylight_time() const;

  inline void set_local_time(const icu4x::IsoDateTime& datetime);

  inline void clear_local_time();

  inline std::unique_ptr<icu4x::IsoDateTime> get_local_time() const;

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
