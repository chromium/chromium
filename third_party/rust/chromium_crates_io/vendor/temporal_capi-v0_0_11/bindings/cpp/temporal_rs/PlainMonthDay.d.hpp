#ifndef temporal_rs_PlainMonthDay_D_HPP
#define temporal_rs_PlainMonthDay_D_HPP

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
namespace capi { struct PlainDate; }
class PlainDate;
namespace capi { struct PlainMonthDay; }
class PlainMonthDay;
namespace capi { struct TimeZone; }
class TimeZone;
struct PartialDate;
struct TemporalError;
class AnyCalendarKind;
class ArithmeticOverflow;
class DisplayCalendar;
}


namespace temporal_rs {
namespace capi {
    struct PlainMonthDay;
} // namespace capi
} // namespace

namespace temporal_rs {
class PlainMonthDay {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> try_new_with_overflow(uint8_t month, uint8_t day, temporal_rs::AnyCalendarKind calendar, temporal_rs::ArithmeticOverflow overflow, std::optional<int32_t> ref_year);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> from_partial(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow);

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> with(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline bool equals(const temporal_rs::PlainMonthDay& other) const;

  inline static int8_t compare(const temporal_rs::PlainMonthDay& one, const temporal_rs::PlainMonthDay& two);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline int32_t iso_year() const;

  inline uint8_t iso_month() const;

  inline uint8_t iso_day() const;

  inline const temporal_rs::Calendar& calendar() const;

  inline std::string month_code() const;
  template<typename W>
  inline void month_code_write(W& writeable_output) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> to_plain_date(std::optional<temporal_rs::PartialDate> year) const;

  inline diplomat::result<int64_t, temporal_rs::TemporalError> epoch_ns_for(const temporal_rs::TimeZone& time_zone) const;

  inline std::string to_ixdtf_string(temporal_rs::DisplayCalendar display_calendar) const;
  template<typename W>
  inline void to_ixdtf_string_write(temporal_rs::DisplayCalendar display_calendar, W& writeable_output) const;

  inline const temporal_rs::capi::PlainMonthDay* AsFFI() const;
  inline temporal_rs::capi::PlainMonthDay* AsFFI();
  inline static const temporal_rs::PlainMonthDay* FromFFI(const temporal_rs::capi::PlainMonthDay* ptr);
  inline static temporal_rs::PlainMonthDay* FromFFI(temporal_rs::capi::PlainMonthDay* ptr);
  inline static void operator delete(void* ptr);
private:
  PlainMonthDay() = delete;
  PlainMonthDay(const temporal_rs::PlainMonthDay&) = delete;
  PlainMonthDay(temporal_rs::PlainMonthDay&&) noexcept = delete;
  PlainMonthDay operator=(const temporal_rs::PlainMonthDay&) = delete;
  PlainMonthDay operator=(temporal_rs::PlainMonthDay&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_PlainMonthDay_D_HPP
