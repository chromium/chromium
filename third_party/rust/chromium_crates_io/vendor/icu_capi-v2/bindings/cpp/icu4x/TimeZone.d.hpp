#ifndef icu4x_TimeZone_D_HPP
#define icu4x_TimeZone_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffset; }
class UtcOffset;
}


namespace icu4x {
namespace capi {
    struct TimeZone;
} // namespace capi
} // namespace

namespace icu4x {
class TimeZone {
public:

  inline static std::unique_ptr<icu4x::TimeZone> unknown();

  inline static std::unique_ptr<icu4x::TimeZone> create_from_bcp47(std::string_view id);

  inline std::unique_ptr<icu4x::TimeZoneInfo> with_offset(const icu4x::UtcOffset& offset) const;

  inline std::unique_ptr<icu4x::TimeZoneInfo> without_offset() const;

  inline const icu4x::capi::TimeZone* AsFFI() const;
  inline icu4x::capi::TimeZone* AsFFI();
  inline static const icu4x::TimeZone* FromFFI(const icu4x::capi::TimeZone* ptr);
  inline static icu4x::TimeZone* FromFFI(icu4x::capi::TimeZone* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZone() = delete;
  TimeZone(const icu4x::TimeZone&) = delete;
  TimeZone(icu4x::TimeZone&&) noexcept = delete;
  TimeZone operator=(const icu4x::TimeZone&) = delete;
  TimeZone operator=(icu4x::TimeZone&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeZone_D_HPP
