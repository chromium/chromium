#ifndef ICU4X_LocaleCanonicalizer_HPP
#define ICU4X_LocaleCanonicalizer_HPP

#include "LocaleCanonicalizer.d.hpp"

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
#include "Locale.hpp"
#include "TransformResult.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::LocaleCanonicalizer* icu4x_LocaleCanonicalizer_create_common_mv1(void);

    typedef struct icu4x_LocaleCanonicalizer_create_common_with_provider_mv1_result {union {icu4x::capi::LocaleCanonicalizer* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleCanonicalizer_create_common_with_provider_mv1_result;
    icu4x_LocaleCanonicalizer_create_common_with_provider_mv1_result icu4x_LocaleCanonicalizer_create_common_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::LocaleCanonicalizer* icu4x_LocaleCanonicalizer_create_extended_mv1(void);

    typedef struct icu4x_LocaleCanonicalizer_create_extended_with_provider_mv1_result {union {icu4x::capi::LocaleCanonicalizer* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleCanonicalizer_create_extended_with_provider_mv1_result;
    icu4x_LocaleCanonicalizer_create_extended_with_provider_mv1_result icu4x_LocaleCanonicalizer_create_extended_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::TransformResult icu4x_LocaleCanonicalizer_canonicalize_mv1(const icu4x::capi::LocaleCanonicalizer* self, icu4x::capi::Locale* locale);

    void icu4x_LocaleCanonicalizer_destroy_mv1(LocaleCanonicalizer* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::LocaleCanonicalizer> icu4x::LocaleCanonicalizer::create_common() {
    auto result = icu4x::capi::icu4x_LocaleCanonicalizer_create_common_mv1();
    return std::unique_ptr<icu4x::LocaleCanonicalizer>(icu4x::LocaleCanonicalizer::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError> icu4x::LocaleCanonicalizer::create_common_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_LocaleCanonicalizer_create_common_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::LocaleCanonicalizer>>(std::unique_ptr<icu4x::LocaleCanonicalizer>(icu4x::LocaleCanonicalizer::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::LocaleCanonicalizer> icu4x::LocaleCanonicalizer::create_extended() {
    auto result = icu4x::capi::icu4x_LocaleCanonicalizer_create_extended_mv1();
    return std::unique_ptr<icu4x::LocaleCanonicalizer>(icu4x::LocaleCanonicalizer::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError> icu4x::LocaleCanonicalizer::create_extended_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_LocaleCanonicalizer_create_extended_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::LocaleCanonicalizer>>(std::unique_ptr<icu4x::LocaleCanonicalizer>(icu4x::LocaleCanonicalizer::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::TransformResult icu4x::LocaleCanonicalizer::canonicalize(icu4x::Locale& locale) const {
    auto result = icu4x::capi::icu4x_LocaleCanonicalizer_canonicalize_mv1(this->AsFFI(),
        locale.AsFFI());
    return icu4x::TransformResult::FromFFI(result);
}

inline const icu4x::capi::LocaleCanonicalizer* icu4x::LocaleCanonicalizer::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::LocaleCanonicalizer*>(this);
}

inline icu4x::capi::LocaleCanonicalizer* icu4x::LocaleCanonicalizer::AsFFI() {
    return reinterpret_cast<icu4x::capi::LocaleCanonicalizer*>(this);
}

inline const icu4x::LocaleCanonicalizer* icu4x::LocaleCanonicalizer::FromFFI(const icu4x::capi::LocaleCanonicalizer* ptr) {
    return reinterpret_cast<const icu4x::LocaleCanonicalizer*>(ptr);
}

inline icu4x::LocaleCanonicalizer* icu4x::LocaleCanonicalizer::FromFFI(icu4x::capi::LocaleCanonicalizer* ptr) {
    return reinterpret_cast<icu4x::LocaleCanonicalizer*>(ptr);
}

inline void icu4x::LocaleCanonicalizer::operator delete(void* ptr) {
    icu4x::capi::icu4x_LocaleCanonicalizer_destroy_mv1(reinterpret_cast<icu4x::capi::LocaleCanonicalizer*>(ptr));
}


#endif // ICU4X_LocaleCanonicalizer_HPP
