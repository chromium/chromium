#ifndef ICU4X_LocaleNamesUnstable_HPP
#define ICU4X_LocaleNamesUnstable_HPP

#include "LocaleNamesUnstable.d.hpp"

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
#include "LanguageDisplayUnstable.hpp"
#include "Locale.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    void icu4x_LocaleNamesUnstable_for_region_light_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_region_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_region_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_region_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_region_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_region_tiny_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_region_tiny_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_region_tiny_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_region_tiny_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_region_tiny_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_region_short_tiny_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_region_short_tiny_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_region_short_tiny_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_region_short_tiny_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_region_short_tiny_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_region_short_light_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_region_short_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_region_short_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_region_short_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_region_short_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView region, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_script_light_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_script_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_script_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_script_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_script_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_script_tiny_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_script_tiny_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_script_tiny_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_script_tiny_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_script_tiny_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_script_heavy_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_script_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_script_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_script_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_script_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_script_short_heavy_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_script_short_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_script_short_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_script_short_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_script_short_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView script, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_for_variant_heavy_mv1(const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView variant, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_variant_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_variant_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_variant_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_variant_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatStringView variant, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_light_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_light_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_light_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_light_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_tiny_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_tiny_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_tiny_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_tiny_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_tiny_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_tiny_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_tiny_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_tiny_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_light_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_light_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_light_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_light_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_long_light_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_long_light_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_long_light_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_long_light_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_long_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_long_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_long_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_long_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_heavy_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_heavy_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_heavy_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_heavy_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_mv1(const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_with_provider_mv1_result {union { icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_with_provider_mv1_result;
    icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_with_provider_mv1_result icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, const icu4x::capi::Locale* langid, icu4x::capi::LanguageDisplayUnstable language_display, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_LocaleNamesUnstable_destroy_mv1(LocaleNamesUnstable* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::string icu4x::LocaleNamesUnstable::for_region_light(const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_light_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_region_light_write(const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_light_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_region_tiny(const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_tiny_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_region_tiny_write(const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_tiny_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_tiny_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_tiny_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_region_short_tiny(const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_tiny_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_region_short_tiny_write(const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_tiny_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_short_tiny_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_short_tiny_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_region_short_light(const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_light_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_region_short_light_write(const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_light_mv1(locale.AsFFI(),
        {region.data(), region.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_short_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_region_short_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view region, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_region_short_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {region.data(), region.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_script_light(const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_light_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_script_light_write(const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_light_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_script_tiny(const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_tiny_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_script_tiny_write(const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_tiny_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_tiny_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_tiny_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_script_heavy(const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_heavy_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_script_heavy_write(const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_heavy_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_script_short_heavy(const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_short_heavy_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_script_short_heavy_write(const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_script_short_heavy_mv1(locale.AsFFI(),
        {script.data(), script.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_short_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_short_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_script_short_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view script, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_script_short_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {script.data(), script.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::LocaleNamesUnstable::for_variant_heavy(const icu4x::Locale& locale, std::string_view variant) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_variant_heavy_mv1(locale.AsFFI(),
        {variant.data(), variant.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::LocaleNamesUnstable::for_variant_heavy_write(const icu4x::Locale& locale, std::string_view variant, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_LocaleNamesUnstable_for_variant_heavy_mv1(locale.AsFFI(),
        {variant.data(), variant.size()},
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_variant_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view variant) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_variant_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {variant.data(), variant.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_variant_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::string_view variant, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_variant_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        {variant.data(), variant.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_light(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_light_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_tiny(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_tiny_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_tiny_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_tiny_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_tiny_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_tiny_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_tiny_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_light(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_light_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_light(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_light_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_light(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_light_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_light(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_light_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_light_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_light_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_light_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_heavy(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_heavy_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_heavy(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_heavy_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_heavy(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_heavy_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_long_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_long_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_heavy(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_heavy_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_menu_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_menu_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_heavy(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_heavy_write(const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_mv1(locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_heavy_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DataError> icu4x::LocaleNamesUnstable::for_language_identifier_short_menu_heavy_with_provider_write(const icu4x::DataProvider& provider, const icu4x::Locale& locale, const icu4x::Locale& langid, icu4x::LanguageDisplayUnstable language_display, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_LocaleNamesUnstable_for_language_identifier_short_menu_heavy_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        langid.AsFFI(),
        language_display.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::LocaleNamesUnstable* icu4x::LocaleNamesUnstable::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::LocaleNamesUnstable*>(this);
}

inline icu4x::capi::LocaleNamesUnstable* icu4x::LocaleNamesUnstable::AsFFI() {
    return reinterpret_cast<icu4x::capi::LocaleNamesUnstable*>(this);
}

inline const icu4x::LocaleNamesUnstable* icu4x::LocaleNamesUnstable::FromFFI(const icu4x::capi::LocaleNamesUnstable* ptr) {
    return reinterpret_cast<const icu4x::LocaleNamesUnstable*>(ptr);
}

inline icu4x::LocaleNamesUnstable* icu4x::LocaleNamesUnstable::FromFFI(icu4x::capi::LocaleNamesUnstable* ptr) {
    return reinterpret_cast<icu4x::LocaleNamesUnstable*>(ptr);
}

inline void icu4x::LocaleNamesUnstable::operator delete(void* ptr) {
    icu4x::capi::icu4x_LocaleNamesUnstable_destroy_mv1(reinterpret_cast<icu4x::capi::LocaleNamesUnstable*>(ptr));
}


#endif // ICU4X_LocaleNamesUnstable_HPP
