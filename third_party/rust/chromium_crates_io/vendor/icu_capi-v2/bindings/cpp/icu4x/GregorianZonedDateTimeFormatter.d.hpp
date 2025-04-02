#ifndef icu4x_GregorianZonedDateTimeFormatter_D_HPP
#define icu4x_GregorianZonedDateTimeFormatter_D_HPP

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
namespace capi { struct GregorianZonedDateTimeFormatter; }
class GregorianZonedDateTimeFormatter;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
class DateTimeFormatError;
class DateTimeFormatterLoadError;
class DateTimeLength;
}


namespace icu4x {
namespace capi {
    struct GregorianZonedDateTimeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class GregorianZonedDateTimeFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length(const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline diplomat::result<std::string, icu4x::DateTimeFormatError> format_iso(const icu4x::IsoDate& date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone) const;

  inline const icu4x::capi::GregorianZonedDateTimeFormatter* AsFFI() const;
  inline icu4x::capi::GregorianZonedDateTimeFormatter* AsFFI();
  inline static const icu4x::GregorianZonedDateTimeFormatter* FromFFI(const icu4x::capi::GregorianZonedDateTimeFormatter* ptr);
  inline static icu4x::GregorianZonedDateTimeFormatter* FromFFI(icu4x::capi::GregorianZonedDateTimeFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  GregorianZonedDateTimeFormatter() = delete;
  GregorianZonedDateTimeFormatter(const icu4x::GregorianZonedDateTimeFormatter&) = delete;
  GregorianZonedDateTimeFormatter(icu4x::GregorianZonedDateTimeFormatter&&) noexcept = delete;
  GregorianZonedDateTimeFormatter operator=(const icu4x::GregorianZonedDateTimeFormatter&) = delete;
  GregorianZonedDateTimeFormatter operator=(icu4x::GregorianZonedDateTimeFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GregorianZonedDateTimeFormatter_D_HPP
