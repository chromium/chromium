#ifndef ICU4X_VariantOffsetsCalculator_D_HPP
#define ICU4X_VariantOffsetsCalculator_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct VariantOffsetsCalculator; }
class VariantOffsetsCalculator;
struct VariantOffsets;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct VariantOffsetsCalculator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `VariantOffsetsCalculator`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.VariantOffsetsCalculator.html) for more information.
 *
 * \deprecated this API is a bad approximation of a time zone database
 */
class [[deprecated("this API is a bad approximation of a time zone database")]] VariantOffsetsCalculator {
public:

  /**
   * Construct a new {@link VariantOffsetsCalculator} instance using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.VariantOffsetsCalculator.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::VariantOffsetsCalculator> create();

  /**
   * Construct a new {@link VariantOffsetsCalculator} instance using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.VariantOffsetsCalculator.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::VariantOffsetsCalculator>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `compute_offsets_from_time_zone_and_name_timestamp`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.VariantOffsetsCalculatorBorrowed.html#method.compute_offsets_from_time_zone_and_name_timestamp) for more information.
   */
  inline std::optional<icu4x::VariantOffsets> compute_offsets_from_time_zone_and_date_time(const icu4x::TimeZone& time_zone, const icu4x::IsoDate& utc_date, const icu4x::Time& utc_time) const;

  /**
   * See the [Rust documentation for `compute_offsets_from_time_zone_and_name_timestamp`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.VariantOffsetsCalculatorBorrowed.html#method.compute_offsets_from_time_zone_and_name_timestamp) for more information.
   */
  inline std::optional<icu4x::VariantOffsets> compute_offsets_from_time_zone_and_timestamp(const icu4x::TimeZone& time_zone, int64_t timestamp) const;

    inline const icu4x::capi::VariantOffsetsCalculator* AsFFI() const;
    inline icu4x::capi::VariantOffsetsCalculator* AsFFI();
    inline static const icu4x::VariantOffsetsCalculator* FromFFI(const icu4x::capi::VariantOffsetsCalculator* ptr);
    inline static icu4x::VariantOffsetsCalculator* FromFFI(icu4x::capi::VariantOffsetsCalculator* ptr);
    inline static void operator delete(void* ptr);
private:
    VariantOffsetsCalculator() = delete;
    VariantOffsetsCalculator(const icu4x::VariantOffsetsCalculator&) = delete;
    VariantOffsetsCalculator(icu4x::VariantOffsetsCalculator&&) noexcept = delete;
    VariantOffsetsCalculator operator=(const icu4x::VariantOffsetsCalculator&) = delete;
    VariantOffsetsCalculator operator=(icu4x::VariantOffsetsCalculator&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_VariantOffsetsCalculator_D_HPP
