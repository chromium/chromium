#ifndef temporal_rs_Calendar_D_HPP
#define temporal_rs_Calendar_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"

namespace temporal_rs {
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Duration; }
class Duration;
namespace capi { struct PlainDate; }
class PlainDate;
namespace capi { struct PlainMonthDay; }
class PlainMonthDay;
namespace capi { struct PlainYearMonth; }
class PlainYearMonth;
struct IsoDate;
struct PartialDate;
struct TemporalError;
class AnyCalendarKind;
class ArithmeticOverflow;
class Unit;
}


namespace temporal_rs {
namespace capi {
    struct Calendar;
} // namespace capi
} // namespace

namespace temporal_rs {
class Calendar {
public:

  inline static std::unique_ptr<temporal_rs::Calendar> create(temporal_rs::AnyCalendarKind kind);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline bool is_iso() const;

  inline std::string_view identifier() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> date_from_partial(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> month_day_from_partial(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> year_month_from_partial(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> date_add(temporal_rs::IsoDate date, const temporal_rs::Duration& duration, temporal_rs::ArithmeticOverflow overflow) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> date_until(temporal_rs::IsoDate one, temporal_rs::IsoDate two, temporal_rs::Unit largest_unit) const;

  inline diplomat::result<std::string, temporal_rs::TemporalError> era(temporal_rs::IsoDate date) const;

  inline std::optional<int32_t> era_year(temporal_rs::IsoDate date) const;

  inline int32_t year(temporal_rs::IsoDate date) const;

  inline uint8_t month(temporal_rs::IsoDate date) const;

  inline diplomat::result<std::string, temporal_rs::TemporalError> month_code(temporal_rs::IsoDate date) const;

  inline uint8_t day(temporal_rs::IsoDate date) const;

  inline diplomat::result<uint16_t, temporal_rs::TemporalError> day_of_week(temporal_rs::IsoDate date) const;

  inline uint16_t day_of_year(temporal_rs::IsoDate date) const;

  inline std::optional<uint8_t> week_of_year(temporal_rs::IsoDate date) const;

  inline std::optional<int32_t> year_of_week(temporal_rs::IsoDate date) const;

  inline diplomat::result<uint16_t, temporal_rs::TemporalError> days_in_week(temporal_rs::IsoDate date) const;

  inline uint16_t days_in_month(temporal_rs::IsoDate date) const;

  inline uint16_t days_in_year(temporal_rs::IsoDate date) const;

  inline uint16_t months_in_year(temporal_rs::IsoDate date) const;

  inline bool in_leap_year(temporal_rs::IsoDate date) const;

  inline const temporal_rs::capi::Calendar* AsFFI() const;
  inline temporal_rs::capi::Calendar* AsFFI();
  inline static const temporal_rs::Calendar* FromFFI(const temporal_rs::capi::Calendar* ptr);
  inline static temporal_rs::Calendar* FromFFI(temporal_rs::capi::Calendar* ptr);
  inline static void operator delete(void* ptr);
private:
  Calendar() = delete;
  Calendar(const temporal_rs::Calendar&) = delete;
  Calendar(temporal_rs::Calendar&&) noexcept = delete;
  Calendar operator=(const temporal_rs::Calendar&) = delete;
  Calendar operator=(temporal_rs::Calendar&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_Calendar_D_HPP
