#ifndef icu4x_RegionDisplayNames_HPP
#define icu4x_RegionDisplayNames_HPP

#include "RegionDisplayNames.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "Locale.hpp"
#include "LocaleParseError.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_RegionDisplayNames_create_mv1_result {union {icu4x::capi::RegionDisplayNames* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_RegionDisplayNames_create_mv1_result;
    icu4x_RegionDisplayNames_create_mv1_result icu4x_RegionDisplayNames_create_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);
    
    typedef struct icu4x_RegionDisplayNames_of_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_RegionDisplayNames_of_mv1_result;
    icu4x_RegionDisplayNames_of_mv1_result icu4x_RegionDisplayNames_of_mv1(const icu4x::capi::RegionDisplayNames* self, diplomat::capi::DiplomatStringView region, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_RegionDisplayNames_destroy_mv1(RegionDisplayNames* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::RegionDisplayNames>, icu4x::DataError> icu4x::RegionDisplayNames::create(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_RegionDisplayNames_create_mv1(provider.AsFFI(),
    locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::RegionDisplayNames>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::RegionDisplayNames>>(std::unique_ptr<icu4x::RegionDisplayNames>(icu4x::RegionDisplayNames::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::RegionDisplayNames>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::string, icu4x::LocaleParseError> icu4x::RegionDisplayNames::of(std::string_view region) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_RegionDisplayNames_of_mv1(this->AsFFI(),
    {region.data(), region.size()},
    &write);
  return result.is_ok ? diplomat::result<std::string, icu4x::LocaleParseError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, icu4x::LocaleParseError>(diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline const icu4x::capi::RegionDisplayNames* icu4x::RegionDisplayNames::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::RegionDisplayNames*>(this);
}

inline icu4x::capi::RegionDisplayNames* icu4x::RegionDisplayNames::AsFFI() {
  return reinterpret_cast<icu4x::capi::RegionDisplayNames*>(this);
}

inline const icu4x::RegionDisplayNames* icu4x::RegionDisplayNames::FromFFI(const icu4x::capi::RegionDisplayNames* ptr) {
  return reinterpret_cast<const icu4x::RegionDisplayNames*>(ptr);
}

inline icu4x::RegionDisplayNames* icu4x::RegionDisplayNames::FromFFI(icu4x::capi::RegionDisplayNames* ptr) {
  return reinterpret_cast<icu4x::RegionDisplayNames*>(ptr);
}

inline void icu4x::RegionDisplayNames::operator delete(void* ptr) {
  icu4x::capi::icu4x_RegionDisplayNames_destroy_mv1(reinterpret_cast<icu4x::capi::RegionDisplayNames*>(ptr));
}


#endif // icu4x_RegionDisplayNames_HPP
