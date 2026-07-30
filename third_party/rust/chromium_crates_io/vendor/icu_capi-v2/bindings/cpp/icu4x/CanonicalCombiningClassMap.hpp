#ifndef ICU4X_CanonicalCombiningClassMap_HPP
#define ICU4X_CanonicalCombiningClassMap_HPP

#include "CanonicalCombiningClassMap.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::CanonicalCombiningClassMap* icu4x_CanonicalCombiningClassMap_create_mv1(void);

    typedef struct icu4x_CanonicalCombiningClassMap_create_with_provider_mv1_result {union {icu4x::capi::CanonicalCombiningClassMap* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CanonicalCombiningClassMap_create_with_provider_mv1_result;
    icu4x_CanonicalCombiningClassMap_create_with_provider_mv1_result icu4x_CanonicalCombiningClassMap_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    uint8_t icu4x_CanonicalCombiningClassMap_get_mv1(const icu4x::capi::CanonicalCombiningClassMap* self, char32_t ch);

    void icu4x_CanonicalCombiningClassMap_destroy_mv1(CanonicalCombiningClassMap* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::CanonicalCombiningClassMap> icu4x::CanonicalCombiningClassMap::create() {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClassMap_create_mv1();
    return std::unique_ptr<icu4x::CanonicalCombiningClassMap>(icu4x::CanonicalCombiningClassMap::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError> icu4x::CanonicalCombiningClassMap::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClassMap_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CanonicalCombiningClassMap>>(std::unique_ptr<icu4x::CanonicalCombiningClassMap>(icu4x::CanonicalCombiningClassMap::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline uint8_t icu4x::CanonicalCombiningClassMap::operator[](char32_t ch) const {
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


#endif // ICU4X_CanonicalCombiningClassMap_HPP
