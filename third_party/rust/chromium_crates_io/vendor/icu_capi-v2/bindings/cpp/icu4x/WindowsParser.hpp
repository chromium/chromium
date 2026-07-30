#ifndef ICU4X_WindowsParser_HPP
#define ICU4X_WindowsParser_HPP

#include "WindowsParser.d.hpp"

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
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::WindowsParser* icu4x_WindowsParser_create_mv1(void);

    typedef struct icu4x_WindowsParser_create_with_provider_mv1_result {union {icu4x::capi::WindowsParser* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WindowsParser_create_with_provider_mv1_result;
    icu4x_WindowsParser_create_with_provider_mv1_result icu4x_WindowsParser_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::TimeZone* icu4x_WindowsParser_parse_mv1(const icu4x::capi::WindowsParser* self, icu4x::diplomat::capi::DiplomatStringView value, icu4x::diplomat::capi::DiplomatStringView region);

    void icu4x_WindowsParser_destroy_mv1(WindowsParser* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::WindowsParser> icu4x::WindowsParser::create() {
    auto result = icu4x::capi::icu4x_WindowsParser_create_mv1();
    return std::unique_ptr<icu4x::WindowsParser>(icu4x::WindowsParser::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WindowsParser>, icu4x::DataError> icu4x::WindowsParser::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_WindowsParser_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WindowsParser>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WindowsParser>>(std::unique_ptr<icu4x::WindowsParser>(icu4x::WindowsParser::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WindowsParser>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::TimeZone> icu4x::WindowsParser::parse(std::string_view value, std::string_view region) const {
    auto result = icu4x::capi::icu4x_WindowsParser_parse_mv1(this->AsFFI(),
        {value.data(), value.size()},
        {region.data(), region.size()});
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline const icu4x::capi::WindowsParser* icu4x::WindowsParser::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::WindowsParser*>(this);
}

inline icu4x::capi::WindowsParser* icu4x::WindowsParser::AsFFI() {
    return reinterpret_cast<icu4x::capi::WindowsParser*>(this);
}

inline const icu4x::WindowsParser* icu4x::WindowsParser::FromFFI(const icu4x::capi::WindowsParser* ptr) {
    return reinterpret_cast<const icu4x::WindowsParser*>(ptr);
}

inline icu4x::WindowsParser* icu4x::WindowsParser::FromFFI(icu4x::capi::WindowsParser* ptr) {
    return reinterpret_cast<icu4x::WindowsParser*>(ptr);
}

inline void icu4x::WindowsParser::operator delete(void* ptr) {
    icu4x::capi::icu4x_WindowsParser_destroy_mv1(reinterpret_cast<icu4x::capi::WindowsParser*>(ptr));
}


#endif // ICU4X_WindowsParser_HPP
