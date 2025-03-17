#ifndef icu4x_DateFormatter_D_HPP
#define icu4x_DateFormatter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Date; }
class Date;
namespace capi { struct DateFormatter; }
class DateFormatter;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
class DateTimeFormatError;
class DateTimeFormatterLoadError;
class DateTimeLength;
}


namespace icu4x {
namespace capi {
    struct DateFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class DateFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length(const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline diplomat::result<std::string, icu4x::DateTimeFormatError> format(const icu4x::Date& value) const;

  inline diplomat::result<std::string, icu4x::DateTimeFormatError> format_iso(const icu4x::IsoDate& value) const;

  inline std::unique_ptr<icu4x::Calendar> calendar() const;

  inline const icu4x::capi::DateFormatter* AsFFI() const;
  inline icu4x::capi::DateFormatter* AsFFI();
  inline static const icu4x::DateFormatter* FromFFI(const icu4x::capi::DateFormatter* ptr);
  inline static icu4x::DateFormatter* FromFFI(icu4x::capi::DateFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  DateFormatter() = delete;
  DateFormatter(const icu4x::DateFormatter&) = delete;
  DateFormatter(icu4x::DateFormatter&&) noexcept = delete;
  DateFormatter operator=(const icu4x::DateFormatter&) = delete;
  DateFormatter operator=(icu4x::DateFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_DateFormatter_D_HPP
