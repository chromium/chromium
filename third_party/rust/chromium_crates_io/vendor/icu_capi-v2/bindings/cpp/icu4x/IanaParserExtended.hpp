#ifndef ICU4X_IanaParserExtended_HPP
#define ICU4X_IanaParserExtended_HPP

#include "IanaParserExtended.d.hpp"

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
#include "TimeZoneAndCanonicalAndNormalized.hpp"
#include "TimeZoneAndCanonicalAndNormalizedIterator.hpp"
#include "TimeZoneAndCanonicalIterator.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::IanaParserExtended* icu4x_IanaParserExtended_create_mv1(void);

    typedef struct icu4x_IanaParserExtended_create_with_provider_mv1_result {union {icu4x::capi::IanaParserExtended* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_IanaParserExtended_create_with_provider_mv1_result;
    icu4x_IanaParserExtended_create_with_provider_mv1_result icu4x_IanaParserExtended_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::TimeZoneAndCanonicalAndNormalized icu4x_IanaParserExtended_parse_mv1(const icu4x::capi::IanaParserExtended* self, icu4x::diplomat::capi::DiplomatStringView value);

    icu4x::capi::TimeZoneAndCanonicalIterator* icu4x_IanaParserExtended_iter_mv1(const icu4x::capi::IanaParserExtended* self);

    icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* icu4x_IanaParserExtended_iter_all_mv1(const icu4x::capi::IanaParserExtended* self);

    void icu4x_IanaParserExtended_destroy_mv1(IanaParserExtended* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::IanaParserExtended> icu4x::IanaParserExtended::create() {
    auto result = icu4x::capi::icu4x_IanaParserExtended_create_mv1();
    return std::unique_ptr<icu4x::IanaParserExtended>(icu4x::IanaParserExtended::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParserExtended>, icu4x::DataError> icu4x::IanaParserExtended::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_IanaParserExtended_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParserExtended>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::IanaParserExtended>>(std::unique_ptr<icu4x::IanaParserExtended>(icu4x::IanaParserExtended::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParserExtended>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::TimeZoneAndCanonicalAndNormalized icu4x::IanaParserExtended::parse(std::string_view value) const {
    auto result = icu4x::capi::icu4x_IanaParserExtended_parse_mv1(this->AsFFI(),
        {value.data(), value.size()});
    return icu4x::TimeZoneAndCanonicalAndNormalized::FromFFI(result);
}

inline std::unique_ptr<icu4x::TimeZoneAndCanonicalIterator> icu4x::IanaParserExtended::iter() const {
    auto result = icu4x::capi::icu4x_IanaParserExtended_iter_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::TimeZoneAndCanonicalIterator>(icu4x::TimeZoneAndCanonicalIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneAndCanonicalAndNormalizedIterator> icu4x::IanaParserExtended::iter_all() const {
    auto result = icu4x::capi::icu4x_IanaParserExtended_iter_all_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::TimeZoneAndCanonicalAndNormalizedIterator>(icu4x::TimeZoneAndCanonicalAndNormalizedIterator::FromFFI(result));
}

inline const icu4x::capi::IanaParserExtended* icu4x::IanaParserExtended::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::IanaParserExtended*>(this);
}

inline icu4x::capi::IanaParserExtended* icu4x::IanaParserExtended::AsFFI() {
    return reinterpret_cast<icu4x::capi::IanaParserExtended*>(this);
}

inline const icu4x::IanaParserExtended* icu4x::IanaParserExtended::FromFFI(const icu4x::capi::IanaParserExtended* ptr) {
    return reinterpret_cast<const icu4x::IanaParserExtended*>(ptr);
}

inline icu4x::IanaParserExtended* icu4x::IanaParserExtended::FromFFI(icu4x::capi::IanaParserExtended* ptr) {
    return reinterpret_cast<icu4x::IanaParserExtended*>(ptr);
}

inline void icu4x::IanaParserExtended::operator delete(void* ptr) {
    icu4x::capi::icu4x_IanaParserExtended_destroy_mv1(reinterpret_cast<icu4x::capi::IanaParserExtended*>(ptr));
}


#endif // ICU4X_IanaParserExtended_HPP
