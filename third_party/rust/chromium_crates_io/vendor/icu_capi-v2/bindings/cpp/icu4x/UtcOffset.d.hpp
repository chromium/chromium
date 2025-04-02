#ifndef icu4x_UtcOffset_D_HPP
#define icu4x_UtcOffset_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct UtcOffset; }
class UtcOffset;
struct TimeZoneInvalidOffsetError;
}


namespace icu4x {
namespace capi {
    struct UtcOffset;
} // namespace capi
} // namespace

namespace icu4x {
class UtcOffset {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError> from_seconds(int32_t seconds);

  inline static std::unique_ptr<icu4x::UtcOffset> from_eighths_of_hour(int8_t eighths_of_hour);

  inline static diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError> from_string(std::string_view offset);

  inline int8_t eighths_of_hour() const;

  inline int32_t seconds() const;

  inline bool is_non_negative() const;

  inline bool is_zero() const;

  inline int32_t hours_part() const;

  inline uint32_t minutes_part() const;

  inline uint32_t seconds_part() const;

  inline const icu4x::capi::UtcOffset* AsFFI() const;
  inline icu4x::capi::UtcOffset* AsFFI();
  inline static const icu4x::UtcOffset* FromFFI(const icu4x::capi::UtcOffset* ptr);
  inline static icu4x::UtcOffset* FromFFI(icu4x::capi::UtcOffset* ptr);
  inline static void operator delete(void* ptr);
private:
  UtcOffset() = delete;
  UtcOffset(const icu4x::UtcOffset&) = delete;
  UtcOffset(icu4x::UtcOffset&&) noexcept = delete;
  UtcOffset operator=(const icu4x::UtcOffset&) = delete;
  UtcOffset operator=(icu4x::UtcOffset&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_UtcOffset_D_HPP
