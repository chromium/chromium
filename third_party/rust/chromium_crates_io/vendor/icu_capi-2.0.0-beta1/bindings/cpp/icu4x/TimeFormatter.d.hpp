#ifndef icu4x_TimeFormatter_D_HPP
#define icu4x_TimeFormatter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct DateTime; }
class DateTime;
namespace capi { struct IsoDateTime; }
class IsoDateTime;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeFormatter; }
class TimeFormatter;
class DateTimeFormatterLoadError;
class DateTimeLength;
}


namespace icu4x {
namespace capi {
    struct TimeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class TimeFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::TimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline std::string format_time(const icu4x::Time& value) const;

  inline std::string format_datetime(const icu4x::DateTime& value) const;

  inline std::string format_iso_datetime(const icu4x::IsoDateTime& value) const;

  inline const icu4x::capi::TimeFormatter* AsFFI() const;
  inline icu4x::capi::TimeFormatter* AsFFI();
  inline static const icu4x::TimeFormatter* FromFFI(const icu4x::capi::TimeFormatter* ptr);
  inline static icu4x::TimeFormatter* FromFFI(icu4x::capi::TimeFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeFormatter() = delete;
  TimeFormatter(const icu4x::TimeFormatter&) = delete;
  TimeFormatter(icu4x::TimeFormatter&&) noexcept = delete;
  TimeFormatter operator=(const icu4x::TimeFormatter&) = delete;
  TimeFormatter operator=(icu4x::TimeFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeFormatter_D_HPP
