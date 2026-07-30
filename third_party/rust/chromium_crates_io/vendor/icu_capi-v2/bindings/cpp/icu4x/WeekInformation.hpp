#ifndef ICU4X_WeekInformation_HPP
#define ICU4X_WeekInformation_HPP

#include "WeekInformation.d.hpp"

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
#include "Locale.hpp"
#include "Weekday.hpp"
#include "WeekdaySetIterator.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_WeekInformation_create_mv1_result {union {icu4x::capi::WeekInformation* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WeekInformation_create_mv1_result;
    icu4x_WeekInformation_create_mv1_result icu4x_WeekInformation_create_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_WeekInformation_create_with_provider_mv1_result {union {icu4x::capi::WeekInformation* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WeekInformation_create_with_provider_mv1_result;
    icu4x_WeekInformation_create_with_provider_mv1_result icu4x_WeekInformation_create_with_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::Weekday icu4x_WeekInformation_first_weekday_mv1(const icu4x::capi::WeekInformation* self);

    bool icu4x_WeekInformation_is_weekend_mv1(const icu4x::capi::WeekInformation* self, icu4x::capi::Weekday day);

    icu4x::capi::WeekdaySetIterator* icu4x_WeekInformation_weekend_mv1(const icu4x::capi::WeekInformation* self);

    void icu4x_WeekInformation_destroy_mv1(WeekInformation* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError> icu4x::WeekInformation::create(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WeekInformation_create_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WeekInformation>>(std::unique_ptr<icu4x::WeekInformation>(icu4x::WeekInformation::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError> icu4x::WeekInformation::create_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WeekInformation_create_with_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WeekInformation>>(std::unique_ptr<icu4x::WeekInformation>(icu4x::WeekInformation::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::Weekday icu4x::WeekInformation::first_weekday() const {
    auto result = icu4x::capi::icu4x_WeekInformation_first_weekday_mv1(this->AsFFI());
    return icu4x::Weekday::FromFFI(result);
}

inline bool icu4x::WeekInformation::is_weekend(icu4x::Weekday day) const {
    auto result = icu4x::capi::icu4x_WeekInformation_is_weekend_mv1(this->AsFFI(),
        day.AsFFI());
    return result;
}

inline std::unique_ptr<icu4x::WeekdaySetIterator> icu4x::WeekInformation::weekend() const {
    auto result = icu4x::capi::icu4x_WeekInformation_weekend_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::WeekdaySetIterator>(icu4x::WeekdaySetIterator::FromFFI(result));
}

inline const icu4x::capi::WeekInformation* icu4x::WeekInformation::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::WeekInformation*>(this);
}

inline icu4x::capi::WeekInformation* icu4x::WeekInformation::AsFFI() {
    return reinterpret_cast<icu4x::capi::WeekInformation*>(this);
}

inline const icu4x::WeekInformation* icu4x::WeekInformation::FromFFI(const icu4x::capi::WeekInformation* ptr) {
    return reinterpret_cast<const icu4x::WeekInformation*>(ptr);
}

inline icu4x::WeekInformation* icu4x::WeekInformation::FromFFI(icu4x::capi::WeekInformation* ptr) {
    return reinterpret_cast<icu4x::WeekInformation*>(ptr);
}

inline void icu4x::WeekInformation::operator delete(void* ptr) {
    icu4x::capi::icu4x_WeekInformation_destroy_mv1(reinterpret_cast<icu4x::capi::WeekInformation*>(ptr));
}


#endif // ICU4X_WeekInformation_HPP
