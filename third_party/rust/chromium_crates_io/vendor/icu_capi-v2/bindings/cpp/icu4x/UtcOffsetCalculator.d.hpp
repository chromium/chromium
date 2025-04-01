#ifndef icu4x_UtcOffsetCalculator_D_HPP
#define icu4x_UtcOffsetCalculator_D_HPP

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
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct UtcOffsetCalculator; }
class UtcOffsetCalculator;
struct UtcOffsets;
class DataError;
}


namespace icu4x {
namespace capi {
    struct UtcOffsetCalculator;
} // namespace capi
} // namespace

namespace icu4x {
class UtcOffsetCalculator {
public:

  inline static std::unique_ptr<icu4x::UtcOffsetCalculator> create();

  inline static diplomat::result<std::unique_ptr<icu4x::UtcOffsetCalculator>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline std::optional<icu4x::UtcOffsets> compute_offsets_from_time_zone(const icu4x::TimeZone& time_zone, const icu4x::IsoDate& local_date, const icu4x::Time& local_time) const;

  inline const icu4x::capi::UtcOffsetCalculator* AsFFI() const;
  inline icu4x::capi::UtcOffsetCalculator* AsFFI();
  inline static const icu4x::UtcOffsetCalculator* FromFFI(const icu4x::capi::UtcOffsetCalculator* ptr);
  inline static icu4x::UtcOffsetCalculator* FromFFI(icu4x::capi::UtcOffsetCalculator* ptr);
  inline static void operator delete(void* ptr);
private:
  UtcOffsetCalculator() = delete;
  UtcOffsetCalculator(const icu4x::UtcOffsetCalculator&) = delete;
  UtcOffsetCalculator(icu4x::UtcOffsetCalculator&&) noexcept = delete;
  UtcOffsetCalculator operator=(const icu4x::UtcOffsetCalculator&) = delete;
  UtcOffsetCalculator operator=(icu4x::UtcOffsetCalculator&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_UtcOffsetCalculator_D_HPP
