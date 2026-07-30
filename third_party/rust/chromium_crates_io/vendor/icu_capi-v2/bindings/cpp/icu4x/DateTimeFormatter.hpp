#ifndef ICU4X_DateTimeFormatter_HPP
#define ICU4X_DateTimeFormatter_HPP

#include "DateTimeFormatter.d.hpp"

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
#include "Time.hpp"
#include "TimePrecision.hpp"
#include "YearStyle.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_DateTimeFormatter_create_dt_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_dt_mv1_result;
    icu4x_DateTimeFormatter_create_dt_mv1_result icu4x_DateTimeFormatter_create_dt_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_dt_with_provider_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_dt_with_provider_mv1_result;
    icu4x_DateTimeFormatter_create_dt_with_provider_mv1_result icu4x_DateTimeFormatter_create_dt_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_mdt_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_mdt_mv1_result;
    icu4x_DateTimeFormatter_create_mdt_mv1_result icu4x_DateTimeFormatter_create_mdt_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_mdt_with_provider_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_mdt_with_provider_mv1_result;
    icu4x_DateTimeFormatter_create_mdt_with_provider_mv1_result icu4x_DateTimeFormatter_create_mdt_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_ymdt_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_ymdt_mv1_result;
    icu4x_DateTimeFormatter_create_ymdt_mv1_result icu4x_DateTimeFormatter_create_ymdt_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateTimeFormatter_create_ymdt_with_provider_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_ymdt_with_provider_mv1_result;
    icu4x_DateTimeFormatter_create_ymdt_with_provider_mv1_result icu4x_DateTimeFormatter_create_ymdt_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateTimeFormatter_create_det_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_det_mv1_result;
    icu4x_DateTimeFormatter_create_det_mv1_result icu4x_DateTimeFormatter_create_det_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_det_with_provider_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_det_with_provider_mv1_result;
    icu4x_DateTimeFormatter_create_det_with_provider_mv1_result icu4x_DateTimeFormatter_create_det_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_mdet_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_mdet_mv1_result;
    icu4x_DateTimeFormatter_create_mdet_mv1_result icu4x_DateTimeFormatter_create_mdet_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_mdet_with_provider_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_mdet_with_provider_mv1_result;
    icu4x_DateTimeFormatter_create_mdet_with_provider_mv1_result icu4x_DateTimeFormatter_create_mdet_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_ymdet_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_ymdet_mv1_result;
    icu4x_DateTimeFormatter_create_ymdet_mv1_result icu4x_DateTimeFormatter_create_ymdet_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateTimeFormatter_create_ymdet_with_provider_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_ymdet_with_provider_mv1_result;
    icu4x_DateTimeFormatter_create_ymdet_with_provider_mv1_result icu4x_DateTimeFormatter_create_ymdet_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment, icu4x::capi::YearStyle_option year_style);

    typedef struct icu4x_DateTimeFormatter_create_et_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_et_mv1_result;
    icu4x_DateTimeFormatter_create_et_mv1_result icu4x_DateTimeFormatter_create_et_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_DateTimeFormatter_create_et_with_provider_mv1_result {union {icu4x::capi::DateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateTimeFormatter_create_et_with_provider_mv1_result;
    icu4x_DateTimeFormatter_create_et_with_provider_mv1_result icu4x_DateTimeFormatter_create_et_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    void icu4x_DateTimeFormatter_format_iso_mv1(const icu4x::capi::DateTimeFormatter* self, const icu4x::capi::IsoDate* iso_date, const icu4x::capi::Time* time, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_DateTimeFormatter_format_same_calendar_mv1_result {union { icu4x::capi::DateTimeMismatchedCalendarError err;}; bool is_ok;} icu4x_DateTimeFormatter_format_same_calendar_mv1_result;
    icu4x_DateTimeFormatter_format_same_calendar_mv1_result icu4x_DateTimeFormatter_format_same_calendar_mv1(const icu4x::capi::DateTimeFormatter* self, const icu4x::capi::Date* date, const icu4x::capi::Time* time, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_DateTimeFormatter_destroy_mv1(DateTimeFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_dt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_dt_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_dt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_dt_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_mdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_mdt_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_mdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_mdt_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_ymdt(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_ymdt_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_ymdt_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_ymdt_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_det(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_det_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_det_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_det_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_mdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_mdet_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_mdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_mdet_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_ymdet(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_ymdet_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_ymdet_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment, std::optional<icu4x::YearStyle> year_style) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_ymdet_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }),
        year_style.has_value() ? (icu4x::capi::YearStyle_option{ { year_style.value().AsFFI() }, true }) : (icu4x::capi::YearStyle_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_et(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_et_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateTimeFormatter::create_et_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_DateTimeFormatter_create_et_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DateTimeFormatter>>(std::unique_ptr<icu4x::DateTimeFormatter>(icu4x::DateTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline std::string icu4x::DateTimeFormatter::format_iso(const icu4x::IsoDate& iso_date, const icu4x::Time& time) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_DateTimeFormatter_format_iso_mv1(this->AsFFI(),
        iso_date.AsFFI(),
        time.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::DateTimeFormatter::format_iso_write(const icu4x::IsoDate& iso_date, const icu4x::Time& time, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_DateTimeFormatter_format_iso_mv1(this->AsFFI(),
        iso_date.AsFFI(),
        time.AsFFI(),
        &write);
}

inline icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError> icu4x::DateTimeFormatter::format_same_calendar(const icu4x::Date& date, const icu4x::Time& time) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_DateTimeFormatter_format_same_calendar_mv1(this->AsFFI(),
        date.AsFFI(),
        time.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Err<icu4x::DateTimeMismatchedCalendarError>(icu4x::DateTimeMismatchedCalendarError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError> icu4x::DateTimeFormatter::format_same_calendar_write(const icu4x::Date& date, const icu4x::Time& time, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_DateTimeFormatter_format_same_calendar_mv1(this->AsFFI(),
        date.AsFFI(),
        time.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DateTimeMismatchedCalendarError>(icu4x::diplomat::Err<icu4x::DateTimeMismatchedCalendarError>(icu4x::DateTimeMismatchedCalendarError::FromFFI(result.err)));
}

inline const icu4x::capi::DateTimeFormatter* icu4x::DateTimeFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::DateTimeFormatter*>(this);
}

inline icu4x::capi::DateTimeFormatter* icu4x::DateTimeFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::DateTimeFormatter*>(this);
}

inline const icu4x::DateTimeFormatter* icu4x::DateTimeFormatter::FromFFI(const icu4x::capi::DateTimeFormatter* ptr) {
    return reinterpret_cast<const icu4x::DateTimeFormatter*>(ptr);
}

inline icu4x::DateTimeFormatter* icu4x::DateTimeFormatter::FromFFI(icu4x::capi::DateTimeFormatter* ptr) {
    return reinterpret_cast<icu4x::DateTimeFormatter*>(ptr);
}

inline void icu4x::DateTimeFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_DateTimeFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::DateTimeFormatter*>(ptr));
}


#endif // ICU4X_DateTimeFormatter_HPP
