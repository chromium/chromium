#ifndef icu4x_UtcOffsetCalculator_HPP
#define icu4x_UtcOffsetCalculator_HPP

#include "UtcOffsetCalculator.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "IsoDate.hpp"
#include "Time.hpp"
#include "TimeZone.hpp"
#include "UtcOffsets.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    icu4x::capi::UtcOffsetCalculator* icu4x_UtcOffsetCalculator_create_mv1(void);
    
    typedef struct icu4x_UtcOffsetCalculator_create_with_provider_mv1_result {union {icu4x::capi::UtcOffsetCalculator* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_UtcOffsetCalculator_create_with_provider_mv1_result;
    icu4x_UtcOffsetCalculator_create_with_provider_mv1_result icu4x_UtcOffsetCalculator_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_UtcOffsetCalculator_compute_offsets_from_time_zone_mv1_result {union {icu4x::capi::UtcOffsets ok; }; bool is_ok;} icu4x_UtcOffsetCalculator_compute_offsets_from_time_zone_mv1_result;
    icu4x_UtcOffsetCalculator_compute_offsets_from_time_zone_mv1_result icu4x_UtcOffsetCalculator_compute_offsets_from_time_zone_mv1(const icu4x::capi::UtcOffsetCalculator* self, const icu4x::capi::TimeZone* time_zone, const icu4x::capi::IsoDate* local_date, const icu4x::capi::Time* local_time);
    
    
    void icu4x_UtcOffsetCalculator_destroy_mv1(UtcOffsetCalculator* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::UtcOffsetCalculator> icu4x::UtcOffsetCalculator::create() {
  auto result = icu4x::capi::icu4x_UtcOffsetCalculator_create_mv1();
  return std::unique_ptr<icu4x::UtcOffsetCalculator>(icu4x::UtcOffsetCalculator::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<icu4x::UtcOffsetCalculator>, icu4x::DataError> icu4x::UtcOffsetCalculator::create_with_provider(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_UtcOffsetCalculator_create_with_provider_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::UtcOffsetCalculator>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::UtcOffsetCalculator>>(std::unique_ptr<icu4x::UtcOffsetCalculator>(icu4x::UtcOffsetCalculator::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::UtcOffsetCalculator>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::optional<icu4x::UtcOffsets> icu4x::UtcOffsetCalculator::compute_offsets_from_time_zone(const icu4x::TimeZone& time_zone, const icu4x::IsoDate& local_date, const icu4x::Time& local_time) const {
  auto result = icu4x::capi::icu4x_UtcOffsetCalculator_compute_offsets_from_time_zone_mv1(this->AsFFI(),
    time_zone.AsFFI(),
    local_date.AsFFI(),
    local_time.AsFFI());
  return result.is_ok ? std::optional<icu4x::UtcOffsets>(icu4x::UtcOffsets::FromFFI(result.ok)) : std::nullopt;
}

inline const icu4x::capi::UtcOffsetCalculator* icu4x::UtcOffsetCalculator::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::UtcOffsetCalculator*>(this);
}

inline icu4x::capi::UtcOffsetCalculator* icu4x::UtcOffsetCalculator::AsFFI() {
  return reinterpret_cast<icu4x::capi::UtcOffsetCalculator*>(this);
}

inline const icu4x::UtcOffsetCalculator* icu4x::UtcOffsetCalculator::FromFFI(const icu4x::capi::UtcOffsetCalculator* ptr) {
  return reinterpret_cast<const icu4x::UtcOffsetCalculator*>(ptr);
}

inline icu4x::UtcOffsetCalculator* icu4x::UtcOffsetCalculator::FromFFI(icu4x::capi::UtcOffsetCalculator* ptr) {
  return reinterpret_cast<icu4x::UtcOffsetCalculator*>(ptr);
}

inline void icu4x::UtcOffsetCalculator::operator delete(void* ptr) {
  icu4x::capi::icu4x_UtcOffsetCalculator_destroy_mv1(reinterpret_cast<icu4x::capi::UtcOffsetCalculator*>(ptr));
}


#endif // icu4x_UtcOffsetCalculator_HPP
