#ifndef icu4x_GregorianDateTimeFormatter_D_HPP
#define icu4x_GregorianDateTimeFormatter_D_HPP

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
namespace capi { struct GregorianDateTimeFormatter; }
class GregorianDateTimeFormatter;
namespace capi { struct IsoDateTime; }
class IsoDateTime;
namespace capi { struct Locale; }
class Locale;
class DateTimeFormatterLoadError;
class DateTimeLength;
}


namespace icu4x {
namespace capi {
    struct GregorianDateTimeFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class GregorianDateTimeFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::GregorianDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> create_with_length(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length);

  inline std::string format_iso_datetime(const icu4x::IsoDateTime& value) const;

  inline const icu4x::capi::GregorianDateTimeFormatter* AsFFI() const;
  inline icu4x::capi::GregorianDateTimeFormatter* AsFFI();
  inline static const icu4x::GregorianDateTimeFormatter* FromFFI(const icu4x::capi::GregorianDateTimeFormatter* ptr);
  inline static icu4x::GregorianDateTimeFormatter* FromFFI(icu4x::capi::GregorianDateTimeFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  GregorianDateTimeFormatter() = delete;
  GregorianDateTimeFormatter(const icu4x::GregorianDateTimeFormatter&) = delete;
  GregorianDateTimeFormatter(icu4x::GregorianDateTimeFormatter&&) noexcept = delete;
  GregorianDateTimeFormatter operator=(const icu4x::GregorianDateTimeFormatter&) = delete;
  GregorianDateTimeFormatter operator=(icu4x::GregorianDateTimeFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GregorianDateTimeFormatter_D_HPP
