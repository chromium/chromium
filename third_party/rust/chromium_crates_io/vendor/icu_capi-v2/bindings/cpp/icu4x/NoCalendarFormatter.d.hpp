#ifndef icu4x_NoCalendarFormatter_D_HPP
#define icu4x_NoCalendarFormatter_D_HPP

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
namespace capi { struct Locale; }
class Locale;
namespace capi { struct NoCalendarFormatter; }
class NoCalendarFormatter;
namespace capi { struct Time; }
class Time;
class DateTimeFormatterLoadError;
class DateTimeLength;
}


namespace icu4x {
namespace capi {
    struct NoCalendarFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class NoCalendarFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length(const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline std::string format(const icu4x::Time& value) const;

  inline const icu4x::capi::NoCalendarFormatter* AsFFI() const;
  inline icu4x::capi::NoCalendarFormatter* AsFFI();
  inline static const icu4x::NoCalendarFormatter* FromFFI(const icu4x::capi::NoCalendarFormatter* ptr);
  inline static icu4x::NoCalendarFormatter* FromFFI(icu4x::capi::NoCalendarFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  NoCalendarFormatter() = delete;
  NoCalendarFormatter(const icu4x::NoCalendarFormatter&) = delete;
  NoCalendarFormatter(icu4x::NoCalendarFormatter&&) noexcept = delete;
  NoCalendarFormatter operator=(const icu4x::NoCalendarFormatter&) = delete;
  NoCalendarFormatter operator=(icu4x::NoCalendarFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_NoCalendarFormatter_D_HPP
