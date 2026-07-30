#ifndef ICU4X_TimeZoneFormatter_HPP
#define ICU4X_TimeZoneFormatter_HPP

#include "TimeZoneFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DataProvider.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeWriteError.hpp"
#include "Locale.hpp"
#include "TimeZoneInfo.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_TimeZoneFormatter_create_specific_long_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_specific_long_mv1_result;
    icu4x_TimeZoneFormatter_create_specific_long_mv1_result icu4x_TimeZoneFormatter_create_specific_long_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_specific_long_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_specific_long_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_specific_long_with_provider_mv1_result icu4x_TimeZoneFormatter_create_specific_long_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_specific_short_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_specific_short_mv1_result;
    icu4x_TimeZoneFormatter_create_specific_short_mv1_result icu4x_TimeZoneFormatter_create_specific_short_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_specific_short_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_specific_short_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_specific_short_with_provider_mv1_result icu4x_TimeZoneFormatter_create_specific_short_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_localized_offset_long_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_localized_offset_long_mv1_result;
    icu4x_TimeZoneFormatter_create_localized_offset_long_mv1_result icu4x_TimeZoneFormatter_create_localized_offset_long_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_localized_offset_long_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_localized_offset_long_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_localized_offset_long_with_provider_mv1_result icu4x_TimeZoneFormatter_create_localized_offset_long_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_localized_offset_short_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_localized_offset_short_mv1_result;
    icu4x_TimeZoneFormatter_create_localized_offset_short_mv1_result icu4x_TimeZoneFormatter_create_localized_offset_short_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_localized_offset_short_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_localized_offset_short_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_localized_offset_short_with_provider_mv1_result icu4x_TimeZoneFormatter_create_localized_offset_short_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_generic_long_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_generic_long_mv1_result;
    icu4x_TimeZoneFormatter_create_generic_long_mv1_result icu4x_TimeZoneFormatter_create_generic_long_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_generic_long_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_generic_long_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_generic_long_with_provider_mv1_result icu4x_TimeZoneFormatter_create_generic_long_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_generic_short_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_generic_short_mv1_result;
    icu4x_TimeZoneFormatter_create_generic_short_mv1_result icu4x_TimeZoneFormatter_create_generic_short_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_generic_short_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_generic_short_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_generic_short_with_provider_mv1_result icu4x_TimeZoneFormatter_create_generic_short_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_location_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_location_mv1_result;
    icu4x_TimeZoneFormatter_create_location_mv1_result icu4x_TimeZoneFormatter_create_location_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_location_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_location_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_location_with_provider_mv1_result icu4x_TimeZoneFormatter_create_location_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_exemplar_city_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_exemplar_city_mv1_result;
    icu4x_TimeZoneFormatter_create_exemplar_city_mv1_result icu4x_TimeZoneFormatter_create_exemplar_city_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_create_exemplar_city_with_provider_mv1_result {union {icu4x::capi::TimeZoneFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeZoneFormatter_create_exemplar_city_with_provider_mv1_result;
    icu4x_TimeZoneFormatter_create_exemplar_city_with_provider_mv1_result icu4x_TimeZoneFormatter_create_exemplar_city_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    typedef struct icu4x_TimeZoneFormatter_format_mv1_result {union { icu4x::capi::DateTimeWriteError err;}; bool is_ok;} icu4x_TimeZoneFormatter_format_mv1_result;
    icu4x_TimeZoneFormatter_format_mv1_result icu4x_TimeZoneFormatter_format_mv1(const icu4x::capi::TimeZoneFormatter* self, const icu4x::capi::TimeZoneInfo* zone, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_TimeZoneFormatter_destroy_mv1(TimeZoneFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_specific_long(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_specific_long_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_specific_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_specific_long_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_specific_short(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_specific_short_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_specific_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_specific_short_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_localized_offset_long(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_localized_offset_long_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_localized_offset_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_localized_offset_long_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_localized_offset_short(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_localized_offset_short_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_localized_offset_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_localized_offset_short_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_generic_long(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_generic_long_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_generic_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_generic_long_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_generic_short(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_generic_short_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_generic_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_generic_short_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_location(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_location_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_location_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_location_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_exemplar_city(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_exemplar_city_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeZoneFormatter::create_exemplar_city_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_create_exemplar_city_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeZoneFormatter>>(std::unique_ptr<icu4x::TimeZoneFormatter>(icu4x::TimeZoneFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeZoneFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError> icu4x::TimeZoneFormatter::format(const icu4x::TimeZoneInfo& zone) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_format_mv1(this->AsFFI(),
        zone.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError>(icu4x::diplomat::Err<icu4x::DateTimeWriteError>(icu4x::DateTimeWriteError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError> icu4x::TimeZoneFormatter::format_write(const icu4x::TimeZoneInfo& zone, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_TimeZoneFormatter_format_mv1(this->AsFFI(),
        zone.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError>(icu4x::diplomat::Err<icu4x::DateTimeWriteError>(icu4x::DateTimeWriteError::FromFFI(result.err)));
}

inline const icu4x::capi::TimeZoneFormatter* icu4x::TimeZoneFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::TimeZoneFormatter*>(this);
}

inline icu4x::capi::TimeZoneFormatter* icu4x::TimeZoneFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::TimeZoneFormatter*>(this);
}

inline const icu4x::TimeZoneFormatter* icu4x::TimeZoneFormatter::FromFFI(const icu4x::capi::TimeZoneFormatter* ptr) {
    return reinterpret_cast<const icu4x::TimeZoneFormatter*>(ptr);
}

inline icu4x::TimeZoneFormatter* icu4x::TimeZoneFormatter::FromFFI(icu4x::capi::TimeZoneFormatter* ptr) {
    return reinterpret_cast<icu4x::TimeZoneFormatter*>(ptr);
}

inline void icu4x::TimeZoneFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_TimeZoneFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZoneFormatter*>(ptr));
}


#endif // ICU4X_TimeZoneFormatter_HPP
