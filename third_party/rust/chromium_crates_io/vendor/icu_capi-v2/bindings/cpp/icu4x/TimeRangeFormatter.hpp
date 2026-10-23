#ifndef ICU4X_TimeRangeFormatter_HPP
#define ICU4X_TimeRangeFormatter_HPP

#include "TimeRangeFormatter.d.hpp"

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
#include "Locale.hpp"
#include "Time.hpp"
#include "TimePrecision.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_TimeRangeFormatter_create_mv1_result {union {icu4x::capi::TimeRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeRangeFormatter_create_mv1_result;
    icu4x_TimeRangeFormatter_create_mv1_result icu4x_TimeRangeFormatter_create_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    typedef struct icu4x_TimeRangeFormatter_create_with_provider_mv1_result {union {icu4x::capi::TimeRangeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeRangeFormatter_create_with_provider_mv1_result;
    icu4x_TimeRangeFormatter_create_with_provider_mv1_result icu4x_TimeRangeFormatter_create_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength_option length, icu4x::capi::TimePrecision_option time_precision, icu4x::capi::DateTimeAlignment_option alignment);

    void icu4x_TimeRangeFormatter_format_mv1(const icu4x::capi::TimeRangeFormatter* self, const icu4x::capi::Time* start_time, const icu4x::capi::Time* end_time, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_TimeRangeFormatter_destroy_mv1(TimeRangeFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeRangeFormatter::create(const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_TimeRangeFormatter_create_mv1(locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeRangeFormatter>>(std::unique_ptr<icu4x::TimeRangeFormatter>(icu4x::TimeRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::TimeRangeFormatter::create_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DateTimeLength> length, std::optional<icu4x::TimePrecision> time_precision, std::optional<icu4x::DateTimeAlignment> alignment) {
    auto result = icu4x::capi::icu4x_TimeRangeFormatter_create_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.has_value() ? (icu4x::capi::DateTimeLength_option{ { length.value().AsFFI() }, true }) : (icu4x::capi::DateTimeLength_option{ {}, false }),
        time_precision.has_value() ? (icu4x::capi::TimePrecision_option{ { time_precision.value().AsFFI() }, true }) : (icu4x::capi::TimePrecision_option{ {}, false }),
        alignment.has_value() ? (icu4x::capi::DateTimeAlignment_option{ { alignment.value().AsFFI() }, true }) : (icu4x::capi::DateTimeAlignment_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TimeRangeFormatter>>(std::unique_ptr<icu4x::TimeRangeFormatter>(icu4x::TimeRangeFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TimeRangeFormatter>, icu4x::DateTimeFormatterLoadError>(icu4x::diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline std::string icu4x::TimeRangeFormatter::format(const icu4x::Time& start_time, const icu4x::Time& end_time) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_TimeRangeFormatter_format_mv1(this->AsFFI(),
        start_time.AsFFI(),
        end_time.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::TimeRangeFormatter::format_write(const icu4x::Time& start_time, const icu4x::Time& end_time, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_TimeRangeFormatter_format_mv1(this->AsFFI(),
        start_time.AsFFI(),
        end_time.AsFFI(),
        &write);
}

inline const icu4x::capi::TimeRangeFormatter* icu4x::TimeRangeFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::TimeRangeFormatter*>(this);
}

inline icu4x::capi::TimeRangeFormatter* icu4x::TimeRangeFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::TimeRangeFormatter*>(this);
}

inline const icu4x::TimeRangeFormatter* icu4x::TimeRangeFormatter::FromFFI(const icu4x::capi::TimeRangeFormatter* ptr) {
    return reinterpret_cast<const icu4x::TimeRangeFormatter*>(ptr);
}

inline icu4x::TimeRangeFormatter* icu4x::TimeRangeFormatter::FromFFI(icu4x::capi::TimeRangeFormatter* ptr) {
    return reinterpret_cast<icu4x::TimeRangeFormatter*>(ptr);
}

inline void icu4x::TimeRangeFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_TimeRangeFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::TimeRangeFormatter*>(ptr));
}


#endif // ICU4X_TimeRangeFormatter_HPP
