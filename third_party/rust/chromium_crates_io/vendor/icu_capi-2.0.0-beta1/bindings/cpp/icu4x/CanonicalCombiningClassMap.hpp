#ifndef icu4x_CanonicalCombiningClassMap_HPP
#define icu4x_CanonicalCombiningClassMap_HPP

#include "CanonicalCombiningClassMap.d.hpp"

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
    
    typedef struct icu4x_CanonicalCombiningClassMap_create_mv1_result {union {icu4x::capi::CanonicalCombiningClassMap* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CanonicalCombiningClassMap_create_mv1_result;
    icu4x_CanonicalCombiningClassMap_create_mv1_result icu4x_CanonicalCombiningClassMap_create_mv1(const icu4x::capi::DataProvider* provider);
    
    uint8_t icu4x_CanonicalCombiningClassMap_get_mv1(const icu4x::capi::CanonicalCombiningClassMap* self, char32_t ch);
    
    
    void icu4x_CanonicalCombiningClassMap_destroy_mv1(CanonicalCombiningClassMap* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError> icu4x::CanonicalCombiningClassMap::create(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CanonicalCombiningClassMap_create_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CanonicalCombiningClassMap>>(std::unique_ptr<icu4x::CanonicalCombiningClassMap>(icu4x::CanonicalCombiningClassMap::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline uint8_t icu4x::CanonicalCombiningClassMap::get(char32_t ch) const {
  auto result = icu4x::capi::icu4x_CanonicalCombiningClassMap_get_mv1(this->AsFFI(),
    ch);
  return result;
}

inline const icu4x::capi::CanonicalCombiningClassMap* icu4x::CanonicalCombiningClassMap::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::CanonicalCombiningClassMap*>(this);
}

inline icu4x::capi::CanonicalCombiningClassMap* icu4x::CanonicalCombiningClassMap::AsFFI() {
  return reinterpret_cast<icu4x::capi::CanonicalCombiningClassMap*>(this);
}

inline const icu4x::CanonicalCombiningClassMap* icu4x::CanonicalCombiningClassMap::FromFFI(const icu4x::capi::CanonicalCombiningClassMap* ptr) {
  return reinterpret_cast<const icu4x::CanonicalCombiningClassMap*>(ptr);
}

inline icu4x::CanonicalCombiningClassMap* icu4x::CanonicalCombiningClassMap::FromFFI(icu4x::capi::CanonicalCombiningClassMap* ptr) {
  return reinterpret_cast<icu4x::CanonicalCombiningClassMap*>(ptr);
}

inline void icu4x::CanonicalCombiningClassMap::operator delete(void* ptr) {
  icu4x::capi::icu4x_CanonicalCombiningClassMap_destroy_mv1(reinterpret_cast<icu4x::capi::CanonicalCombiningClassMap*>(ptr));
}


#endif // icu4x_CanonicalCombiningClassMap_HPP
