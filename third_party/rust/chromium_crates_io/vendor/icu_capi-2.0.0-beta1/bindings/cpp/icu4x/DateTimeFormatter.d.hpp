#ifndef icu4x_DateTimeFormatter_D_HPP
#define icu4x_DateTimeFormatter_D_HPP

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
namespace capi { struct DateTimeFormatter; }
class DateTimeFormatter;
namespace capi { struct IsoDateTime; }
class IsoDateTime;
namespace capi { struct Locale; }
class Locale;
class DateTimeFormatError;
class DateTimeFormatterLoadError;
class DateTimeLength;
}


namespace icu4x {
namespace capi {
    struct DateTimeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class DateTimeFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline diplomat::result<std::string, icu4x::DateTimeFormatError> format_datetime(const icu4x::DateTime& value) const;

  inline diplomat::result<std::string, icu4x::DateTimeFormatError> format_iso_datetime(const icu4x::IsoDateTime& value) const;

  inline const icu4x::capi::DateTimeFormatter* AsFFI() const;
  inline icu4x::capi::DateTimeFormatter* AsFFI();
  inline static const icu4x::DateTimeFormatter* FromFFI(const icu4x::capi::DateTimeFormatter* ptr);
  inline static icu4x::DateTimeFormatter* FromFFI(icu4x::capi::DateTimeFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  DateTimeFormatter() = delete;
  DateTimeFormatter(const icu4x::DateTimeFormatter&) = delete;
  DateTimeFormatter(icu4x::DateTimeFormatter&&) noexcept = delete;
  DateTimeFormatter operator=(const icu4x::DateTimeFormatter&) = delete;
  DateTimeFormatter operator=(icu4x::DateTimeFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_DateTimeFormatter_D_HPP
