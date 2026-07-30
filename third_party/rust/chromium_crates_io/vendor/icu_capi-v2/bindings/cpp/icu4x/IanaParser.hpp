#ifndef ICU4X_IanaParser_HPP
#define ICU4X_IanaParser_HPP

#include "IanaParser.d.hpp"

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
#include "TimeZone.hpp"
#include "TimeZoneIterator.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::IanaParser* icu4x_IanaParser_create_mv1(void);

    typedef struct icu4x_IanaParser_create_with_provider_mv1_result {union {icu4x::capi::IanaParser* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_IanaParser_create_with_provider_mv1_result;
    icu4x_IanaParser_create_with_provider_mv1_result icu4x_IanaParser_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::TimeZone* icu4x_IanaParser_parse_mv1(const icu4x::capi::IanaParser* self, icu4x::diplomat::capi::DiplomatStringView value);

    icu4x::capi::TimeZoneIterator* icu4x_IanaParser_iter_mv1(const icu4x::capi::IanaParser* self);

    void icu4x_IanaParser_destroy_mv1(IanaParser* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::IanaParser> icu4x::IanaParser::create() {
    auto result = icu4x::capi::icu4x_IanaParser_create_mv1();
    return std::unique_ptr<icu4x::IanaParser>(icu4x::IanaParser::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParser>, icu4x::DataError> icu4x::IanaParser::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_IanaParser_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParser>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::IanaParser>>(std::unique_ptr<icu4x::IanaParser>(icu4x::IanaParser::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParser>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::TimeZone> icu4x::IanaParser::parse(std::string_view value) const {
    auto result = icu4x::capi::icu4x_IanaParser_parse_mv1(this->AsFFI(),
        {value.data(), value.size()});
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneIterator> icu4x::IanaParser::iter() const {
    auto result = icu4x::capi::icu4x_IanaParser_iter_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::TimeZoneIterator>(icu4x::TimeZoneIterator::FromFFI(result));
}

inline const icu4x::capi::IanaParser* icu4x::IanaParser::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::IanaParser*>(this);
}

inline icu4x::capi::IanaParser* icu4x::IanaParser::AsFFI() {
    return reinterpret_cast<icu4x::capi::IanaParser*>(this);
}

inline const icu4x::IanaParser* icu4x::IanaParser::FromFFI(const icu4x::capi::IanaParser* ptr) {
    return reinterpret_cast<const icu4x::IanaParser*>(ptr);
}

inline icu4x::IanaParser* icu4x::IanaParser::FromFFI(icu4x::capi::IanaParser* ptr) {
    return reinterpret_cast<icu4x::IanaParser*>(ptr);
}

inline void icu4x::IanaParser::operator delete(void* ptr) {
    icu4x::capi::icu4x_IanaParser_destroy_mv1(reinterpret_cast<icu4x::capi::IanaParser*>(ptr));
}


#endif // ICU4X_IanaParser_HPP
