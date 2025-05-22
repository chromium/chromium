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
struct PartialDate;
struct TemporalError;
class ArithmeticOverflow;
}


namespace temporal_rs {
namespace capi {
    struct PlainMonthDay;
} // namespace capi
} // namespace

namespace temporal_rs {
class PlainMonthDay {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> create_with_overflow(uint8_t month, uint8_t day, const temporal_rs::Calendar& calendar, temporal_rs::ArithmeticOverflow overflow, std::optional<int32_t> ref_year);

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> with(temporal_rs::PartialDate partial, temporal_rs::ArithmeticOverflow overflow) const;

  inline bool equals(const temporal_rs::PlainMonthDay& other) const;

  inline static int8_t compare(const temporal_rs::PlainMonthDay& one, const temporal_rs::PlainMonthDay& two);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline int32_t iso_year() const;

  inline uint8_t iso_month() const;

  inline uint8_t iso_day() const;

  inline const temporal_rs::Calendar& calendar() const;

  inline std::string month_code() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> to_plain_date() const;

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
