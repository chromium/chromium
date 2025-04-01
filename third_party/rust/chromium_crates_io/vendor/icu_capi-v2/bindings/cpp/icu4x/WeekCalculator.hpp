#ifndef icu4x_WeekCalculator_HPP
#define icu4x_WeekCalculator_HPP

#include "WeekCalculator.d.hpp"

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
#include "Locale.hpp"
#include "Weekday.hpp"
#include "WeekendContainsDay.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_WeekCalculator_create_mv1_result {union {icu4x::capi::WeekCalculator* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WeekCalculator_create_mv1_result;
    icu4x_WeekCalculator_create_mv1_result icu4x_WeekCalculator_create_mv1(const icu4x::capi::Locale* locale);
    
    typedef struct icu4x_WeekCalculator_create_with_provider_mv1_result {union {icu4x::capi::WeekCalculator* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WeekCalculator_create_with_provider_mv1_result;
    icu4x_WeekCalculator_create_with_provider_mv1_result icu4x_WeekCalculator_create_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);
    
    icu4x::capi::WeekCalculator* icu4x_WeekCalculator_from_first_day_of_week_and_min_week_days_mv1(icu4x::capi::Weekday first_weekday, uint8_t min_week_days);
    
    icu4x::capi::Weekday icu4x_WeekCalculator_first_weekday_mv1(const icu4x::capi::WeekCalculator* self);
    
    uint8_t icu4x_WeekCalculator_min_week_days_mv1(const icu4x::capi::WeekCalculator* self);
    
    icu4x::capi::WeekendContainsDay icu4x_WeekCalculator_weekend_mv1(const icu4x::capi::WeekCalculator* self);
    
    
    void icu4x_WeekCalculator_destroy_mv1(WeekCalculator* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::WeekCalculator>, icu4x::DataError> icu4x::WeekCalculator::create(const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_WeekCalculator_create_mv1(locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::WeekCalculator>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::WeekCalculator>>(std::unique_ptr<icu4x::WeekCalculator>(icu4x::WeekCalculator::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::WeekCalculator>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::WeekCalculator>, icu4x::DataError> icu4x::WeekCalculator::create_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_WeekCalculator_create_with_provider_mv1(provider.AsFFI(),
    locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::WeekCalculator>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::WeekCalculator>>(std::unique_ptr<icu4x::WeekCalculator>(icu4x::WeekCalculator::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::WeekCalculator>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::WeekCalculator> icu4x::WeekCalculator::from_first_day_of_week_and_min_week_days(icu4x::Weekday first_weekday, uint8_t min_week_days) {
  auto result = icu4x::capi::icu4x_WeekCalculator_from_first_day_of_week_and_min_week_days_mv1(first_weekday.AsFFI(),
    min_week_days);
  return std::unique_ptr<icu4x::WeekCalculator>(icu4x::WeekCalculator::FromFFI(result));
}

inline icu4x::Weekday icu4x::WeekCalculator::first_weekday() const {
  auto result = icu4x::capi::icu4x_WeekCalculator_first_weekday_mv1(this->AsFFI());
  return icu4x::Weekday::FromFFI(result);
}

inline uint8_t icu4x::WeekCalculator::min_week_days() const {
  auto result = icu4x::capi::icu4x_WeekCalculator_min_week_days_mv1(this->AsFFI());
  return result;
}

inline icu4x::WeekendContainsDay icu4x::WeekCalculator::weekend() const {
  auto result = icu4x::capi::icu4x_WeekCalculator_weekend_mv1(this->AsFFI());
  return icu4x::WeekendContainsDay::FromFFI(result);
}

inline const icu4x::capi::WeekCalculator* icu4x::WeekCalculator::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::WeekCalculator*>(this);
}

inline icu4x::capi::WeekCalculator* icu4x::WeekCalculator::AsFFI() {
  return reinterpret_cast<icu4x::capi::WeekCalculator*>(this);
}

inline const icu4x::WeekCalculator* icu4x::WeekCalculator::FromFFI(const icu4x::capi::WeekCalculator* ptr) {
  return reinterpret_cast<const icu4x::WeekCalculator*>(ptr);
}

inline icu4x::WeekCalculator* icu4x::WeekCalculator::FromFFI(icu4x::capi::WeekCalculator* ptr) {
  return reinterpret_cast<icu4x::WeekCalculator*>(ptr);
}

inline void icu4x::WeekCalculator::operator delete(void* ptr) {
  icu4x::capi::icu4x_WeekCalculator_destroy_mv1(reinterpret_cast<icu4x::capi::WeekCalculator*>(ptr));
}


#endif // icu4x_WeekCalculator_HPP
