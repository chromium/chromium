#ifndef icu4x_TimeZoneIdMapper_HPP
#define icu4x_TimeZoneIdMapper_HPP

#include "TimeZoneIdMapper.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_TimeZoneIdMapper_create_mv1_result {union {icu4x::capi::TimeZoneIdMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_TimeZoneIdMapper_create_mv1_result;
    icu4x_TimeZoneIdMapper_create_mv1_result icu4x_TimeZoneIdMapper_create_mv1(const icu4x::capi::DataProvider* provider);
    
    void icu4x_TimeZoneIdMapper_iana_to_bcp47_mv1(const icu4x::capi::TimeZoneIdMapper* self, diplomat::capi::DiplomatStringView value, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_TimeZoneIdMapper_normalize_iana_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapper_normalize_iana_mv1_result;
    icu4x_TimeZoneIdMapper_normalize_iana_mv1_result icu4x_TimeZoneIdMapper_normalize_iana_mv1(const icu4x::capi::TimeZoneIdMapper* self, diplomat::capi::DiplomatStringView value, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_TimeZoneIdMapper_canonicalize_iana_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapper_canonicalize_iana_mv1_result;
    icu4x_TimeZoneIdMapper_canonicalize_iana_mv1_result icu4x_TimeZoneIdMapper_canonicalize_iana_mv1(const icu4x::capi::TimeZoneIdMapper* self, diplomat::capi::DiplomatStringView value, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1_result;
    icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1_result icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1(const icu4x::capi::TimeZoneIdMapper* self, diplomat::capi::DiplomatStringView value, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_TimeZoneIdMapper_destroy_mv1(TimeZoneIdMapper* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapper>, icu4x::DataError> icu4x::TimeZoneIdMapper::create(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_TimeZoneIdMapper_create_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::TimeZoneIdMapper>>(std::unique_ptr<icu4x::TimeZoneIdMapper>(icu4x::TimeZoneIdMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::TimeZoneIdMapper::iana_to_bcp47(std::string_view value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_TimeZoneIdMapper_iana_to_bcp47_mv1(this->AsFFI(),
    {value.data(), value.size()},
    &write);
  return output;
}

inline diplomat::result<std::optional<std::string>, diplomat::Utf8Error> icu4x::TimeZoneIdMapper::normalize_iana(std::string_view value) const {
  if (!diplomat::capi::diplomat_is_str(value.data(), value.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_TimeZoneIdMapper_normalize_iana_mv1(this->AsFFI(),
    {value.data(), value.size()},
    &write);
  return diplomat::Ok<std::optional<std::string>>(result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt);
}

inline diplomat::result<std::optional<std::string>, diplomat::Utf8Error> icu4x::TimeZoneIdMapper::canonicalize_iana(std::string_view value) const {
  if (!diplomat::capi::diplomat_is_str(value.data(), value.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_TimeZoneIdMapper_canonicalize_iana_mv1(this->AsFFI(),
    {value.data(), value.size()},
    &write);
  return diplomat::Ok<std::optional<std::string>>(result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt);
}

inline std::optional<std::string> icu4x::TimeZoneIdMapper::find_canonical_iana_from_bcp47(std::string_view value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1(this->AsFFI(),
    {value.data(), value.size()},
    &write);
  return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}

inline const icu4x::capi::TimeZoneIdMapper* icu4x::TimeZoneIdMapper::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::TimeZoneIdMapper*>(this);
}

inline icu4x::capi::TimeZoneIdMapper* icu4x::TimeZoneIdMapper::AsFFI() {
  return reinterpret_cast<icu4x::capi::TimeZoneIdMapper*>(this);
}

inline const icu4x::TimeZoneIdMapper* icu4x::TimeZoneIdMapper::FromFFI(const icu4x::capi::TimeZoneIdMapper* ptr) {
  return reinterpret_cast<const icu4x::TimeZoneIdMapper*>(ptr);
}

inline icu4x::TimeZoneIdMapper* icu4x::TimeZoneIdMapper::FromFFI(icu4x::capi::TimeZoneIdMapper* ptr) {
  return reinterpret_cast<icu4x::TimeZoneIdMapper*>(ptr);
}

inline void icu4x::TimeZoneIdMapper::operator delete(void* ptr) {
  icu4x::capi::icu4x_TimeZoneIdMapper_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZoneIdMapper*>(ptr));
}


#endif // icu4x_TimeZoneIdMapper_HPP
