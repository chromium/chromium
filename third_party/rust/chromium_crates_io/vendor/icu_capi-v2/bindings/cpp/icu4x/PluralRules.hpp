#ifndef ICU4X_PluralRules_HPP
#define ICU4X_PluralRules_HPP

#include "PluralRules.d.hpp"

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
#include "PluralCategories.hpp"
#include "PluralCategory.hpp"
#include "PluralOperands.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_PluralRules_create_cardinal_mv1_result {union {icu4x::capi::PluralRules* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRules_create_cardinal_mv1_result;
    icu4x_PluralRules_create_cardinal_mv1_result icu4x_PluralRules_create_cardinal_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_PluralRules_create_cardinal_with_provider_mv1_result {union {icu4x::capi::PluralRules* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRules_create_cardinal_with_provider_mv1_result;
    icu4x_PluralRules_create_cardinal_with_provider_mv1_result icu4x_PluralRules_create_cardinal_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_PluralRules_create_ordinal_mv1_result {union {icu4x::capi::PluralRules* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRules_create_ordinal_mv1_result;
    icu4x_PluralRules_create_ordinal_mv1_result icu4x_PluralRules_create_ordinal_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_PluralRules_create_ordinal_with_provider_mv1_result {union {icu4x::capi::PluralRules* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRules_create_ordinal_with_provider_mv1_result;
    icu4x_PluralRules_create_ordinal_with_provider_mv1_result icu4x_PluralRules_create_ordinal_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::PluralCategory icu4x_PluralRules_category_for_mv1(const icu4x::capi::PluralRules* self, const icu4x::capi::PluralOperands* op);

    icu4x::capi::PluralCategories icu4x_PluralRules_categories_mv1(const icu4x::capi::PluralRules* self);

    void icu4x_PluralRules_destroy_mv1(PluralRules* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> icu4x::PluralRules::create_cardinal(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRules_create_cardinal_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRules>>(std::unique_ptr<icu4x::PluralRules>(icu4x::PluralRules::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> icu4x::PluralRules::create_cardinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRules_create_cardinal_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRules>>(std::unique_ptr<icu4x::PluralRules>(icu4x::PluralRules::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> icu4x::PluralRules::create_ordinal(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRules_create_ordinal_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRules>>(std::unique_ptr<icu4x::PluralRules>(icu4x::PluralRules::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> icu4x::PluralRules::create_ordinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRules_create_ordinal_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRules>>(std::unique_ptr<icu4x::PluralRules>(icu4x::PluralRules::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::PluralCategory icu4x::PluralRules::category_for(const icu4x::PluralOperands& op) const {
    auto result = icu4x::capi::icu4x_PluralRules_category_for_mv1(this->AsFFI(),
        op.AsFFI());
    return icu4x::PluralCategory::FromFFI(result);
}

inline icu4x::PluralCategories icu4x::PluralRules::categories() const {
    auto result = icu4x::capi::icu4x_PluralRules_categories_mv1(this->AsFFI());
    return icu4x::PluralCategories::FromFFI(result);
}

inline const icu4x::capi::PluralRules* icu4x::PluralRules::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::PluralRules*>(this);
}

inline icu4x::capi::PluralRules* icu4x::PluralRules::AsFFI() {
    return reinterpret_cast<icu4x::capi::PluralRules*>(this);
}

inline const icu4x::PluralRules* icu4x::PluralRules::FromFFI(const icu4x::capi::PluralRules* ptr) {
    return reinterpret_cast<const icu4x::PluralRules*>(ptr);
}

inline icu4x::PluralRules* icu4x::PluralRules::FromFFI(icu4x::capi::PluralRules* ptr) {
    return reinterpret_cast<icu4x::PluralRules*>(ptr);
}

inline void icu4x::PluralRules::operator delete(void* ptr) {
    icu4x::capi::icu4x_PluralRules_destroy_mv1(reinterpret_cast<icu4x::capi::PluralRules*>(ptr));
}


#endif // ICU4X_PluralRules_HPP
