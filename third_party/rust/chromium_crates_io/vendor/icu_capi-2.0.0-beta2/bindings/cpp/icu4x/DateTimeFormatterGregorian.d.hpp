#ifndef icu4x_DateTimeFormatterGregorian_D_HPP
#define icu4x_DateTimeFormatterGregorian_D_HPP

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
namespace capi { struct DateTimeFormatterGregorian; }
class DateTimeFormatterGregorian;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct Time; }
class Time;
class DateTimeAlignment;
class DateTimeFormatterLoadError;
class DateTimeLength;
class TimePrecision;
class YearStyle;
}


namespace icu4x {
namespace capi {
    struct DateTimeFormatterGregorian;
} // namespace capi
} // namespace

namespace icu4x {
class DateTimeFormatterGregorian {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_dt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_dt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_det(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_det_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_mdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_ymdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_et(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline static diplomat::result<std::unique_ptr<icu4x::DateTimeFormatterGregorian>, icu4x::DateTimeFormatterLoadError> create_et_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment);

  inline std::string format_iso(const icu4x::IsoDate& date, const icu4x::Time& time) const;

  inline const icu4x::capi::DateTimeFormatterGregorian* AsFFI() const;
  inline icu4x::capi::DateTimeFormatterGregorian* AsFFI();
  inline static const icu4x::DateTimeFormatterGregorian* FromFFI(const icu4x::capi::DateTimeFormatterGregorian* ptr);
  inline static icu4x::DateTimeFormatterGregorian* FromFFI(icu4x::capi::DateTimeFormatterGregorian* ptr);
  inline static void operator delete(void* ptr);
private:
  DateTimeFormatterGregorian() = delete;
  DateTimeFormatterGregorian(const icu4x::DateTimeFormatterGregorian&) = delete;
  DateTimeFormatterGregorian(icu4x::DateTimeFormatterGregorian&&) noexcept = delete;
  DateTimeFormatterGregorian operator=(const icu4x::DateTimeFormatterGregorian&) = delete;
  DateTimeFormatterGregorian operator=(icu4x::DateTimeFormatterGregorian&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_DateTimeFormatterGregorian_D_HPP
