#ifndef ICU4X_ZonedTimeFormatter_HPP
#define ICU4X_ZonedTimeFormatter_HPP

#include "ZonedTimeFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DataProvider.hpp"
#include "DateTimeAlignment.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeLength.hpp"
#include "DateTimeWriteError.hpp"
#include "Locale.hpp"
#include "Time.hpp"
#include "TimePrecision.hpp"
#include "TimeZoneInfo.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_ZonedTimeFormatter_create_specific_long_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_specific_long_mv1_result;
    icu4x_ZonedTimeFormatter_create_specific_long_mv1_result icu4x_ZonedTimeFormatter_create_specific_long_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_specific_long_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_specific_long_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_specific_long_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_specific_long_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_specific_short_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_specific_short_mv1_result;
    icu4x_ZonedTimeFormatter_create_specific_short_mv1_result icu4x_ZonedTimeFormatter_create_specific_short_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_specific_short_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_specific_short_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_specific_short_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_specific_short_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_localized_offset_long_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_localized_offset_long_mv1_result;
    icu4x_ZonedTimeFormatter_create_localized_offset_long_mv1_result icu4x_ZonedTimeFormatter_create_localized_offset_long_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_localized_offset_long_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_localized_offset_long_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_localized_offset_long_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_localized_offset_long_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_localized_offset_short_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_localized_offset_short_mv1_result;
    icu4x_ZonedTimeFormatter_create_localized_offset_short_mv1_result icu4x_ZonedTimeFormatter_create_localized_offset_short_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_localized_offset_short_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_localized_offset_short_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_localized_offset_short_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_localized_offset_short_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_generic_long_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_generic_long_mv1_result;
    icu4x_ZonedTimeFormatter_create_generic_long_mv1_result icu4x_ZonedTimeFormatter_create_generic_long_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_generic_long_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_generic_long_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_generic_long_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_generic_long_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_generic_short_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_generic_short_mv1_result;
    icu4x_ZonedTimeFormatter_create_generic_short_mv1_result icu4x_ZonedTimeFormatter_create_generic_short_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_generic_short_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_generic_short_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_generic_short_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_generic_short_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_location_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_location_mv1_result;
    icu4x_ZonedTimeFormatter_create_location_mv1_result icu4x_ZonedTimeFormatter_create_location_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_location_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_location_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_location_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_location_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_exemplar_city_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_exemplar_city_mv1_result;
    icu4x_ZonedTimeFormatter_create_exemplar_city_mv1_result icu4x_ZonedTimeFormatter_create_exemplar_city_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_create_exemplar_city_with_provider_mv1_result {union {icu4x::capi::ZonedTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_create_exemplar_city_with_provider_mv1_result;
    icu4x_ZonedTimeFormatter_create_exemplar_city_with_provider_mv1_result icu4x_ZonedTimeFormatter_create_exemplar_city_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_ZonedTimeFormatter_format_mv1_result {union { icu4x::capi::DateTimeWriteError err;}; bool is_ok;} icu4x_ZonedTimeFormatter_format_mv1_result;
    icu4x_ZonedTimeFormatter_format_mv1_result icu4x_ZonedTimeFormatter_format_mv1(const icu4x::capi::ZonedTimeFormatter* self, const icu4x::capi::Time* time, const icu4x::capi::TimeZoneInfo* zone, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_ZonedTimeFormatter_destroy_mv1(ZonedTimeFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_specific_long(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_specific_long_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_specific_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_specific_long_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_specific_short(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_specific_short_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_specific_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_specific_short_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_localized_offset_long(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_localized_offset_long_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_localized_offset_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_localized_offset_long_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_localized_offset_short(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_localized_offset_short_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_localized_offset_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_localized_offset_short_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_generic_long(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_generic_long_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_generic_long_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_generic_long_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_generic_short(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_generic_short_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_generic_short_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_generic_short_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_location(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_location_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_location_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_location_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_exemplar_city(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_exemplar_city_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedTimeFormatter::create_exemplar_city_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_create_exemplar_city_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ZonedTimeFormatter>>(std::unique_ptr<icu4x::ZonedTimeFormatter>(icu4x::ZonedTimeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ZonedTimeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError> icu4x::ZonedTimeFormatter::format(const icu4x::Time& time, const icu4x::TimeZoneInfo& zone) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_format_mv1(this->AsFFI(),
        time.AsFFI(),
        zone.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::DateTimeWriteError>(icu4x::diplomat::Err<icu4x::DateTimeWriteError>(icu4x::DateTimeWriteError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError> icu4x::ZonedTimeFormatter::format_write(const icu4x::Time& time, const icu4x::TimeZoneInfo& zone, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_ZonedTimeFormatter_format_mv1(this->AsFFI(),
        time.AsFFI(),
        zone.AsFFI(),
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::DateTimeWriteError>(icu4x::diplomat::Err<icu4x::DateTimeWriteError>(icu4x::DateTimeWriteError::FromFFI(result.err)));
}

inline const icu4x::capi::ZonedTimeFormatter* icu4x::ZonedTimeFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::ZonedTimeFormatter*>(this);
}

inline icu4x::capi::ZonedTimeFormatter* icu4x::ZonedTimeFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::ZonedTimeFormatter*>(this);
}

inline const icu4x::ZonedTimeFormatter* icu4x::ZonedTimeFormatter::FromFFI(const icu4x::capi::ZonedTimeFormatter* ptr) {
    return reinterpret_cast<const icu4x::ZonedTimeFormatter*>(ptr);
}

inline icu4x::ZonedTimeFormatter* icu4x::ZonedTimeFormatter::FromFFI(icu4x::capi::ZonedTimeFormatter* ptr) {
    return reinterpret_cast<icu4x::ZonedTimeFormatter*>(ptr);
}

inline void icu4x::ZonedTimeFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_ZonedTimeFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::ZonedTimeFormatter*>(ptr));
}


#endif // ICU4X_ZonedTimeFormatter_HPP
