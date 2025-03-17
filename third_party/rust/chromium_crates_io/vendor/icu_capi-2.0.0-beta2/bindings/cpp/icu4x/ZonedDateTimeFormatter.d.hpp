#ifndef icu4x_ZonedDateTimeFormatter_D_HPP
#define icu4x_ZonedDateTimeFormatter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Date; }
class Date;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct ZonedDateTimeFormatter; }
class ZonedDateTimeFormatter;
class DateTimeFormatError;
class DateTimeFormatterLoadError;
class DateTimeLength;
}


namespace icu4x {
namespace capi {
    struct ZonedDateTimeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class ZonedDateTimeFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length(const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline diplomat::result<std::string, icu4x::DateTimeFormatError> format(const icu4x::Date& date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone) const;

  inline diplomat::result<std::string, icu4x::DateTimeFormatError> format_iso(const icu4x::IsoDate& date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone) const;

  inline const icu4x::capi::ZonedDateTimeFormatter* AsFFI() const;
  inline icu4x::capi::ZonedDateTimeFormatter* AsFFI();
  inline static const icu4x::ZonedDateTimeFormatter* FromFFI(const icu4x::capi::ZonedDateTimeFormatter* ptr);
  inline static icu4x::ZonedDateTimeFormatter* FromFFI(icu4x::capi::ZonedDateTimeFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  ZonedDateTimeFormatter() = delete;
  ZonedDateTimeFormatter(const icu4x::ZonedDateTimeFormatter&) = delete;
  ZonedDateTimeFormatter(icu4x::ZonedDateTimeFormatter&&) noexcept = delete;
  ZonedDateTimeFormatter operator=(const icu4x::ZonedDateTimeFormatter&) = delete;
  ZonedDateTimeFormatter operator=(icu4x::ZonedDateTimeFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_ZonedDateTimeFormatter_D_HPP
