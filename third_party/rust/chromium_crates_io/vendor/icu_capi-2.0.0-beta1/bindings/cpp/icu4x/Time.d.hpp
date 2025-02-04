#ifndef icu4x_Time_D_HPP
#define icu4x_Time_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Time; }
class Time;
class CalendarError;
class CalendarParseError;
}


namespace icu4x {
namespace capi {
    struct Time;
} // namespace capi
} // namespace

namespace icu4x {
class Time {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> create(uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond);

  inline static diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarParseError> from_string(std::string_view v);

  inline static diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> midnight();

  inline uint8_t hour() const;

  inline uint8_t minute() const;

  inline uint8_t second() const;

  inline uint32_t nanosecond() const;

  inline const icu4x::capi::Time* AsFFI() const;
  inline icu4x::capi::Time* AsFFI();
  inline static const icu4x::Time* FromFFI(const icu4x::capi::Time* ptr);
  inline static icu4x::Time* FromFFI(icu4x::capi::Time* ptr);
  inline static void operator delete(void* ptr);
private:
  Time() = delete;
  Time(const icu4x::Time&) = delete;
  Time(icu4x::Time&&) noexcept = delete;
  Time operator=(const icu4x::Time&) = delete;
  Time operator=(icu4x::Time&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_Time_D_HPP
