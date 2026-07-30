#ifndef ICU4X_DateFormatter_HPP
#define ICU4X_DateFormatter_HPP

#include "DateFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DataProvider.hpp"
#include "Date.hpp"
#include "DateTimeAlignment.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeLength.hpp"
#include "DateTimeMismatchedCalendarError.hpp"
#include "IsoDate.hpp"
#include "Locale.hpp"
#include "YearStyle.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_DateFormatter_create_d_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_d_mv1_result;
    icu4x_DateFormatter_create_d_mv1_result icu4x_DateFormatter_create_d_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_d_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_d_with_provider_mv1_result;
    icu4x_DateFormatter_create_d_with_provider_mv1_result icu4x_DateFormatter_create_d_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_md_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_md_mv1_result;
    icu4x_DateFormatter_create_md_mv1_result icu4x_DateFormatter_create_md_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_md_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_md_with_provider_mv1_result;
    icu4x_DateFormatter_create_md_with_provider_mv1_result icu4x_DateFormatter_create_md_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_ymd_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_ymd_mv1_result;
    icu4x_DateFormatter_create_ymd_mv1_result icu4x_DateFormatter_create_ymd_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateFormatter_create_ymd_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_ymd_with_provider_mv1_result;
    icu4x_DateFormatter_create_ymd_with_provider_mv1_result icu4x_DateFormatter_create_ymd_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateFormatter_create_de_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_de_mv1_result;
    icu4x_DateFormatter_create_de_mv1_result icu4x_DateFormatter_create_de_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_de_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_de_with_provider_mv1_result;
    icu4x_DateFormatter_create_de_with_provider_mv1_result icu4x_DateFormatter_create_de_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_mde_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_mde_mv1_result;
    icu4x_DateFormatter_create_mde_mv1_result icu4x_DateFormatter_create_mde_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_mde_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_mde_with_provider_mv1_result;
    icu4x_DateFormatter_create_mde_with_provider_mv1_result icu4x_DateFormatter_create_mde_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_ymde_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_ymde_mv1_result;
    icu4x_DateFormatter_create_ymde_mv1_result icu4x_DateFormatter_create_ymde_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateFormatter_create_ymde_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_ymde_with_provider_mv1_result;
    icu4x_DateFormatter_create_ymde_with_provider_mv1_result icu4x_DateFormatter_create_ymde_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateFormatter_create_e_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_e_mv1_result;
    icu4x_DateFormatter_create_e_mv1_result icu4x_DateFormatter_create_e_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length);

    typedef struct icu4x_DateFormatter_create_e_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_e_with_provider_mv1_result;
    icu4x_DateFormatter_create_e_with_provider_mv1_result icu4x_DateFormatter_create_e_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length);

    typedef struct icu4x_DateFormatter_create_m_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_m_mv1_result;
    icu4x_DateFormatter_create_m_mv1_result icu4x_DateFormatter_create_m_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_m_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_m_with_provider_mv1_result;
    icu4x_DateFormatter_create_m_with_provider_mv1_result icu4x_DateFormatter_create_m_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateFormatter_create_ym_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_ym_mv1_result;
    icu4x_DateFormatter_create_ym_mv1_result icu4x_DateFormatter_create_ym_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateFormatter_create_ym_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_ym_with_provider_mv1_result;
    icu4x_DateFormatter_create_ym_with_provider_mv1_result icu4x_DateFormatter_create_ym_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateFormatter_create_y_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_y_mv1_result;
    icu4x_DateFormatter_create_y_mv1_result icu4x_DateFormatter_create_y_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateFormatter_create_y_with_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_y_with_provider_mv1_result;
    icu4x_DateFormatter_create_y_with_provider_mv1_result icu4x_DateFormatter_create_y_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    void icu4x_DateFormatter_format_iso_mv1(const icu4x::capi::DateFormatter* self, const icu4x::capi::IsoDate* iso_date, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_DateFormatter_format_same_calendar_mv1_result {union { icu4x::capi::DateTimeMismatchedCalendarError err;}; bool is_ok;} icu4x_DateFormatter_format_same_calendar_mv1_result;
    icu4x_DateFormatter_format_same_calendar_mv1_result icu4x_DateFormatter_format_same_calendar_mv1(const icu4x::capi::DateFormatter* self, const icu4x::capi::Date* date, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_DateFormatter_destroy_mv1(DateFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_d(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_d_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_d_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_d_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_md(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_md_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_md_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_md_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_ymd(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_ymd_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_ymd_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_ymd_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_de(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_de_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_de_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_de_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_mde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_mde_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_mde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_mde_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_ymde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_ymde_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_ymde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_ymde_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_e(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_e_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_e_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_e_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_m(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_m_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_m_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_m_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_ym(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_ym_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_ym_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_ym_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_y(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_y_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_y_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateFormatter_create_y_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline std::string icu4x::DateFormatter::format_iso(const icu4x::IsoDate& iso_date) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_DateFormatter_format_iso_mv1(this->AsFFI(),
        iso_date.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::DateFormatter::format_iso_write(const icu4x::IsoDate& iso_date, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_DateFormatter_format_iso_mv1(this->AsFFI(),
        iso_date.AsFFI(),
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError> icu4x::DateFormatter::format_same_calendar(const icu4x::Date& date) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_DateFormatter_format_same_calendar_mv1(this->AsFFI(),
        date.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Err<icu4x::DateTimeMismatchedCalendarError>(icu4x::DateTimeMismatchedCalendarError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError> icu4x::DateFormatter::format_same_calendar_write(const icu4x::Date& date, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_DateFormatter_format_same_calendar_mv1(this->AsFFI(),
        date.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Err<icu4x::DateTimeMismatchedCalendarError>(icu4x::DateTimeMismatchedCalendarError::FromFFI(result.err)));
}

inline const icu4x::capi::DateFormatter* icu4x::DateFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::DateFormatter*>(this);
}

inline icu4x::capi::DateFormatter* icu4x::DateFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::DateFormatter*>(this);
}

inline const icu4x::DateFormatter* icu4x::DateFormatter::FromFFI(const icu4x::capi::DateFormatter* ptr) {
    return reinterpret_cast<const icu4x::DateFormatter*>(ptr);
}

inline icu4x::DateFormatter* icu4x::DateFormatter::FromFFI(icu4x::capi::DateFormatter* ptr) {
    return reinterpret_cast<icu4x::DateFormatter*>(ptr);
}

inline void icu4x::DateFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_DateFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::DateFormatter*>(ptr));
}


#endif // ICU4X_DateFormatter_HPP
