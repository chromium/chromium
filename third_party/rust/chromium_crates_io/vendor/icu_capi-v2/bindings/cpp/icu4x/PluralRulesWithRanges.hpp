#ifndef ICU4X_PluralRulesWithRanges_HPP
#define ICU4X_PluralRulesWithRanges_HPP

#include "PluralRulesWithRanges.d.hpp"

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
#include "PluralCategory.hpp"
#include "PluralOperands.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_PluralRulesWithRanges_create_cardinal_mv1_result {union {icu4x::capi::PluralRulesWithRanges* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_cardinal_mv1_result;
    icu4x_PluralRulesWithRanges_create_cardinal_mv1_result icu4x_PluralRulesWithRanges_create_cardinal_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1_result {union {icu4x::capi::PluralRulesWithRanges* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1_result;
    icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1_result icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_PluralRulesWithRanges_create_ordinal_mv1_result {union {icu4x::capi::PluralRulesWithRanges* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_ordinal_mv1_result;
    icu4x_PluralRulesWithRanges_create_ordinal_mv1_result icu4x_PluralRulesWithRanges_create_ordinal_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1_result {union {icu4x::capi::PluralRulesWithRanges* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1_result;
    icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1_result icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::PluralCategory icu4x_PluralRulesWithRanges_category_for_range_mv1(const icu4x::capi::PluralRulesWithRanges* self, const icu4x::capi::PluralOperands* start, const icu4x::capi::PluralOperands* end);

    void icu4x_PluralRulesWithRanges_destroy_mv1(PluralRulesWithRanges* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> icu4x::PluralRulesWithRanges::create_cardinal(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRulesWithRanges_create_cardinal_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRulesWithRanges>>(std::unique_ptr<icu4x::PluralRulesWithRanges>(icu4x::PluralRulesWithRanges::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> icu4x::PluralRulesWithRanges::create_cardinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRulesWithRanges>>(std::unique_ptr<icu4x::PluralRulesWithRanges>(icu4x::PluralRulesWithRanges::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> icu4x::PluralRulesWithRanges::create_ordinal(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRulesWithRanges_create_ordinal_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRulesWithRanges>>(std::unique_ptr<icu4x::PluralRulesWithRanges>(icu4x::PluralRulesWithRanges::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> icu4x::PluralRulesWithRanges::create_ordinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralRulesWithRanges>>(std::unique_ptr<icu4x::PluralRulesWithRanges>(icu4x::PluralRulesWithRanges::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::PluralCategory icu4x::PluralRulesWithRanges::category_for_range(const icu4x::PluralOperands& start, const icu4x::PluralOperands& end) const {
    auto result = icu4x::capi::icu4x_PluralRulesWithRanges_category_for_range_mv1(this->AsFFI(),
        start.AsFFI(),
        end.AsFFI());
    return icu4x::PluralCategory::FromFFI(result);
}

inline const icu4x::capi::PluralRulesWithRanges* icu4x::PluralRulesWithRanges::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::PluralRulesWithRanges*>(this);
}

inline icu4x::capi::PluralRulesWithRanges* icu4x::PluralRulesWithRanges::AsFFI() {
    return reinterpret_cast<icu4x::capi::PluralRulesWithRanges*>(this);
}

inline const icu4x::PluralRulesWithRanges* icu4x::PluralRulesWithRanges::FromFFI(const icu4x::capi::PluralRulesWithRanges* ptr) {
    return reinterpret_cast<const icu4x::PluralRulesWithRanges*>(ptr);
}

inline icu4x::PluralRulesWithRanges* icu4x::PluralRulesWithRanges::FromFFI(icu4x::capi::PluralRulesWithRanges* ptr) {
    return reinterpret_cast<icu4x::PluralRulesWithRanges*>(ptr);
}

inline void icu4x::PluralRulesWithRanges::operator delete(void* ptr) {
    icu4x::capi::icu4x_PluralRulesWithRanges_destroy_mv1(reinterpret_cast<icu4x::capi::PluralRulesWithRanges*>(ptr));
}


#endif // ICU4X_PluralRulesWithRanges_HPP
