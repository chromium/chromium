#ifndef icu4x_TimeZoneIdMapperWithFastCanonicalization_HPP
#define icu4x_TimeZoneIdMapperWithFastCanonicalization_HPP

#include "TimeZoneIdMapperWithFastCanonicalization.d.hpp"

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
    
    typedef struct icu4x_TimeZoneIdMapperWithFastCanonicalization_create_mv1_result {union {icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_TimeZoneIdMapperWithFastCanonicalization_create_mv1_result;
    icu4x_TimeZoneIdMapperWithFastCanonicalization_create_mv1_result icu4x_TimeZoneIdMapperWithFastCanonicalization_create_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_TimeZoneIdMapperWithFastCanonicalization_canonicalize_iana_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapperWithFastCanonicalization_canonicalize_iana_mv1_result;
    icu4x_TimeZoneIdMapperWithFastCanonicalization_canonicalize_iana_mv1_result icu4x_TimeZoneIdMapperWithFastCanonicalization_canonicalize_iana_mv1(const icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* self, diplomat::capi::DiplomatStringView value, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_TimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47_mv1_result;
    icu4x_TimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47_mv1_result icu4x_TimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47_mv1(const icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* self, diplomat::capi::DiplomatStringView value, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_TimeZoneIdMapperWithFastCanonicalization_destroy_mv1(TimeZoneIdMapperWithFastCanonicalization* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapperWithFastCanonicalization>, icu4x::DataError> icu4x::TimeZoneIdMapperWithFastCanonicalization::create(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_TimeZoneIdMapperWithFastCanonicalization_create_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapperWithFastCanonicalization>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::TimeZoneIdMapperWithFastCanonicalization>>(std::unique_ptr<icu4x::TimeZoneIdMapperWithFastCanonicalization>(icu4x::TimeZoneIdMapperWithFastCanonicalization::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapperWithFastCanonicalization>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::optional<std::string>, diplomat::Utf8Error> icu4x::TimeZoneIdMapperWithFastCanonicalization::canonicalize_iana(std::string_view value) const {
  if (!diplomat::capi::diplomat_is_str(value.data(), value.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_TimeZoneIdMapperWithFastCanonicalization_canonicalize_iana_mv1(this->AsFFI(),
    {value.data(), value.size()},
    &write);
  return diplomat::Ok<std::optional<std::string>>(result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt);
}

inline std::optional<std::string> icu4x::TimeZoneIdMapperWithFastCanonicalization::canonical_iana_from_bcp47(std::string_view value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_TimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47_mv1(this->AsFFI(),
    {value.data(), value.size()},
    &write);
  return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}

inline const icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* icu4x::TimeZoneIdMapperWithFastCanonicalization::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::TimeZoneIdMapperWithFastCanonicalization*>(this);
}

inline icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* icu4x::TimeZoneIdMapperWithFastCanonicalization::AsFFI() {
  return reinterpret_cast<icu4x::capi::TimeZoneIdMapperWithFastCanonicalization*>(this);
}

inline const icu4x::TimeZoneIdMapperWithFastCanonicalization* icu4x::TimeZoneIdMapperWithFastCanonicalization::FromFFI(const icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* ptr) {
  return reinterpret_cast<const icu4x::TimeZoneIdMapperWithFastCanonicalization*>(ptr);
}

inline icu4x::TimeZoneIdMapperWithFastCanonicalization* icu4x::TimeZoneIdMapperWithFastCanonicalization::FromFFI(icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* ptr) {
  return reinterpret_cast<icu4x::TimeZoneIdMapperWithFastCanonicalization*>(ptr);
}

inline void icu4x::TimeZoneIdMapperWithFastCanonicalization::operator delete(void* ptr) {
  icu4x::capi::icu4x_TimeZoneIdMapperWithFastCanonicalization_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZoneIdMapperWithFastCanonicalization*>(ptr));
}


#endif // icu4x_TimeZoneIdMapperWithFastCanonicalization_HPP
