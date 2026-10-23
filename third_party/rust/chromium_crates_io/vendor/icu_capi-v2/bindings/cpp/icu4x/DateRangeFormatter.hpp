#ifndef ICU4X_DateRangeFormatter_HPP
#define ICU4X_DateRangeFormatter_HPP

#include "DateRangeFormatter.d.hpp"

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
#include "IsoDate.hpp"
#include "Locale.hpp"
#include "YearStyle.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_DateRangeFormatter_create_d_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_d_mv1_result;
    icu4x_DateRangeFormatter_create_d_mv1_result icu4x_DateRangeFormatter_create_d_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_d_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_d_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_d_with_provider_mv1_result icu4x_DateRangeFormatter_create_d_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_md_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_md_mv1_result;
    icu4x_DateRangeFormatter_create_md_mv1_result icu4x_DateRangeFormatter_create_md_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_md_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_md_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_md_with_provider_mv1_result icu4x_DateRangeFormatter_create_md_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_ymd_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymd_mv1_result;
    icu4x_DateRangeFormatter_create_ymd_mv1_result icu4x_DateRangeFormatter_create_ymd_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateRangeFormatter_create_ymd_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymd_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_ymd_with_provider_mv1_result icu4x_DateRangeFormatter_create_ymd_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateRangeFormatter_create_de_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_de_mv1_result;
    icu4x_DateRangeFormatter_create_de_mv1_result icu4x_DateRangeFormatter_create_de_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_de_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_de_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_de_with_provider_mv1_result icu4x_DateRangeFormatter_create_de_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_mde_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_mde_mv1_result;
    icu4x_DateRangeFormatter_create_mde_mv1_result icu4x_DateRangeFormatter_create_mde_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_mde_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_mde_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_mde_with_provider_mv1_result icu4x_DateRangeFormatter_create_mde_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_ymde_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymde_mv1_result;
    icu4x_DateRangeFormatter_create_ymde_mv1_result icu4x_DateRangeFormatter_create_ymde_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateRangeFormatter_create_ymde_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymde_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_ymde_with_provider_mv1_result icu4x_DateRangeFormatter_create_ymde_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateRangeFormatter_create_e_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_e_mv1_result;
    icu4x_DateRangeFormatter_create_e_mv1_result icu4x_DateRangeFormatter_create_e_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length);

    typedef struct icu4x_DateRangeFormatter_create_e_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_e_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_e_with_provider_mv1_result icu4x_DateRangeFormatter_create_e_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length);

    typedef struct icu4x_DateRangeFormatter_create_m_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_m_mv1_result;
    icu4x_DateRangeFormatter_create_m_mv1_result icu4x_DateRangeFormatter_create_m_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_m_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_m_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_m_with_provider_mv1_result icu4x_DateRangeFormatter_create_m_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateRangeFormatter_create_ym_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ym_mv1_result;
    icu4x_DateRangeFormatter_create_ym_mv1_result icu4x_DateRangeFormatter_create_ym_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateRangeFormatter_create_ym_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ym_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_ym_with_provider_mv1_result icu4x_DateRangeFormatter_create_ym_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateRangeFormatter_create_y_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_y_mv1_result;
    icu4x_DateRangeFormatter_create_y_mv1_result icu4x_DateRangeFormatter_create_y_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateRangeFormatter_create_y_with_provider_mv1_result {union {icu4x::capi::DateRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_y_with_provider_mv1_result;
    icu4x_DateRangeFormatter_create_y_with_provider_mv1_result icu4x_DateRangeFormatter_create_y_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    void icu4x_DateRangeFormatter_format_iso_mv1(const icu4x::capi::DateRangeFormatter* self, const icu4x::capi::IsoDate* start_iso_date, const icu4x::capi::IsoDate* end_iso_date, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_DateRangeFormatter_format_same_calendar_mv1(const icu4x::capi::DateRangeFormatter* self, const icu4x::capi::Date* start_date, const icu4x::capi::Date* end_date, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_DateRangeFormatter_destroy_mv1(DateRangeFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_d(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_d_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_d_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_d_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_md(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_md_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_md_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_md_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_ymd(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_ymd_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_ymd_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_ymd_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_de(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_de_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_de_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_de_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_mde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_mde_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_mde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_mde_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_ymde(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_ymde_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_ymde_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_ymde_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_e(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_e_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_e_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_e_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_m(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_m_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_m_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_m_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_ym(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_ym_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_ym_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_ym_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_y(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_y_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateRangeFormatter::create_y_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateRangeFormatter_create_y_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateRangeFormatter>>(std::unique_ptr<icu4x::DateRangeFormatter>(icu4x::DateRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline std::string icu4x::DateRangeFormatter::format_iso(const icu4x::IsoDate& start_iso_date, const icu4x::IsoDate& end_iso_date) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_DateRangeFormatter_format_iso_mv1(this->AsFFI(),
        start_iso_date.AsFFI(),
        end_iso_date.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::DateRangeFormatter::format_iso_write(const icu4x::IsoDate& start_iso_date, const icu4x::IsoDate& end_iso_date, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_DateRangeFormatter_format_iso_mv1(this->AsFFI(),
        start_iso_date.AsFFI(),
        end_iso_date.AsFFI(),
        &write);
}

inline std::string icu4x::DateRangeFormatter::format_same_calendar(const icu4x::Date& start_date, const icu4x::Date& end_date) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_DateRangeFormatter_format_same_calendar_mv1(this->AsFFI(),
        start_date.AsFFI(),
        end_date.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::DateRangeFormatter::format_same_calendar_write(const icu4x::Date& start_date, const icu4x::Date& end_date, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_DateRangeFormatter_format_same_calendar_mv1(this->AsFFI(),
        start_date.AsFFI(),
        end_date.AsFFI(),
        &write);
}

inline const icu4x::capi::DateRangeFormatter* icu4x::DateRangeFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::DateRangeFormatter*>(this);
}

inline icu4x::capi::DateRangeFormatter* icu4x::DateRangeFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::DateRangeFormatter*>(this);
}

inline const icu4x::DateRangeFormatter* icu4x::DateRangeFormatter::FromFFI(const icu4x::capi::DateRangeFormatter* ptr) {
    return reinterpret_cast<const icu4x::DateRangeFormatter*>(ptr);
}

inline icu4x::DateRangeFormatter* icu4x::DateRangeFormatter::FromFFI(icu4x::capi::DateRangeFormatter* ptr) {
    return reinterpret_cast<icu4x::DateRangeFormatter*>(ptr);
}

inline void icu4x::DateRangeFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_DateRangeFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::DateRangeFormatter*>(ptr));
}


#endif // ICU4X_DateRangeFormatter_HPP
