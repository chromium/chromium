#ifndef icu4x_WeekCalculator_D_HPP
#define icu4x_WeekCalculator_D_HPP

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
namespace capi { struct Locale; }
class Locale;
namespace capi { struct WeekCalculator; }
class WeekCalculator;
struct WeekendContainsDay;
class DataError;
class IsoWeekday;
}


namespace icu4x {
namespace capi {
    struct WeekCalculator;
} // namespace capi
} // namespace

namespace icu4x {
class WeekCalculator {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::WeekCalculator>, icu4x::DataError> create(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline static std::unique_ptr<icu4x::WeekCalculator> from_first_day_of_week_and_min_week_days(icu4x::IsoWeekday first_weekday, uint8_t min_week_days);

  inline icu4x::IsoWeekday first_weekday() const;

  inline uint8_t min_week_days() const;

  inline icu4x::WeekendContainsDay weekend() const;

  inline const icu4x::capi::WeekCalculator* AsFFI() const;
  inline icu4x::capi::WeekCalculator* AsFFI();
  inline static const icu4x::WeekCalculator* FromFFI(const icu4x::capi::WeekCalculator* ptr);
  inline static icu4x::WeekCalculator* FromFFI(icu4x::capi::WeekCalculator* ptr);
  inline static void operator delete(void* ptr);
private:
  WeekCalculator() = delete;
  WeekCalculator(const icu4x::WeekCalculator&) = delete;
  WeekCalculator(icu4x::WeekCalculator&&) noexcept = delete;
  WeekCalculator operator=(const icu4x::WeekCalculator&) = delete;
  WeekCalculator operator=(icu4x::WeekCalculator&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_WeekCalculator_D_HPP
