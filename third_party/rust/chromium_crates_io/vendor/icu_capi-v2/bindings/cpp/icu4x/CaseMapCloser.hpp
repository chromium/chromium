#ifndef ICU4X_CaseMapCloser_HPP
#define ICU4X_CaseMapCloser_HPP

#include "CaseMapCloser.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointSetBuilder.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_CaseMapCloser_create_mv1_result {union {icu4x::capi::CaseMapCloser* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CaseMapCloser_create_mv1_result;
    icu4x_CaseMapCloser_create_mv1_result icu4x_CaseMapCloser_create_mv1(void);

    typedef struct icu4x_CaseMapCloser_create_with_provider_mv1_result {union {icu4x::capi::CaseMapCloser* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CaseMapCloser_create_with_provider_mv1_result;
    icu4x_CaseMapCloser_create_with_provider_mv1_result icu4x_CaseMapCloser_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    void icu4x_CaseMapCloser_add_case_closure_to_mv1(const icu4x::capi::CaseMapCloser* self, char32_t c, icu4x::capi::CodePointSetBuilder* builder);

    bool icu4x_CaseMapCloser_add_string_case_closure_to_mv1(const icu4x::capi::CaseMapCloser* self, icu4x::diplomat::capi::DiplomatStringView s, icu4x::capi::CodePointSetBuilder* builder);

    void icu4x_CaseMapCloser_destroy_mv1(CaseMapCloser* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError> icu4x::CaseMapCloser::create() {
    auto result = icu4x::capi::icu4x_CaseMapCloser_create_mv1();
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CaseMapCloser>>(std::unique_ptr<icu4x::CaseMapCloser>(icu4x::CaseMapCloser::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError> icu4x::CaseMapCloser::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CaseMapCloser_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CaseMapCloser>>(std::unique_ptr<icu4x::CaseMapCloser>(icu4x::CaseMapCloser::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline void icu4x::CaseMapCloser::add_case_closure_to(char32_t c, icu4x::CodePointSetBuilder& builder) const {
    icu4x::capi::icu4x_CaseMapCloser_add_case_closure_to_mv1(this->AsFFI(),
        c,
        builder.AsFFI());
}

inline bool icu4x::CaseMapCloser::add_string_case_closure_to(std::string_view s, icu4x::CodePointSetBuilder& builder) const {
    auto result = icu4x::capi::icu4x_CaseMapCloser_add_string_case_closure_to_mv1(this->AsFFI(),
        {s.data(), s.size()},
        builder.AsFFI());
    return result;
}

inline const icu4x::capi::CaseMapCloser* icu4x::CaseMapCloser::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::CaseMapCloser*>(this);
}

inline icu4x::capi::CaseMapCloser* icu4x::CaseMapCloser::AsFFI() {
    return reinterpret_cast<icu4x::capi::CaseMapCloser*>(this);
}

inline const icu4x::CaseMapCloser* icu4x::CaseMapCloser::FromFFI(const icu4x::capi::CaseMapCloser* ptr) {
    return reinterpret_cast<const icu4x::CaseMapCloser*>(ptr);
}

inline icu4x::CaseMapCloser* icu4x::CaseMapCloser::FromFFI(icu4x::capi::CaseMapCloser* ptr) {
    return reinterpret_cast<icu4x::CaseMapCloser*>(ptr);
}

inline void icu4x::CaseMapCloser::operator delete(void* ptr) {
    icu4x::capi::icu4x_CaseMapCloser_destroy_mv1(reinterpret_cast<icu4x::capi::CaseMapCloser*>(ptr));
}


#endif // ICU4X_CaseMapCloser_HPP
