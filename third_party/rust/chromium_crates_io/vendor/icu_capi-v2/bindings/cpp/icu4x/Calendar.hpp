#ifndef ICU4X_Calendar_HPP
#define ICU4X_Calendar_HPP

#include "Calendar.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CalendarKind.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::Calendar* icu4x_Calendar_create_mv1(icu4x::capi::CalendarKind kind);

    typedef struct icu4x_Calendar_create_with_provider_mv1_result {union {icu4x::capi::Calendar* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_Calendar_create_with_provider_mv1_result;
    icu4x_Calendar_create_with_provider_mv1_result icu4x_Calendar_create_with_provider_mv1(const icu4x::capi::DataProvider* provider, icu4x::capi::CalendarKind kind);

    icu4x::capi::CalendarKind icu4x_Calendar_kind_mv1(const icu4x::capi::Calendar* self);

    void icu4x_Calendar_destroy_mv1(Calendar* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::Calendar> icu4x::Calendar::create(icu4x::CalendarKind kind) {
    auto result = icu4x::capi::icu4x_Calendar_create_mv1(kind.AsFFI());
    return std::unique_ptr<icu4x::Calendar>(icu4x::Calendar::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Calendar>, icu4x::DataError> icu4x::Calendar::create_with_provider(const icu4x::DataProvider& provider, icu4x::CalendarKind kind) {
    auto result = icu4x::capi::icu4x_Calendar_create_with_provider_mv1(provider.AsFFI(),
        kind.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Calendar>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Calendar>>(std::unique_ptr<icu4x::Calendar>(icu4x::Calendar::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Calendar>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::CalendarKind icu4x::Calendar::kind() const {
    auto result = icu4x::capi::icu4x_Calendar_kind_mv1(this->AsFFI());
    return icu4x::CalendarKind::FromFFI(result);
}

inline const icu4x::capi::Calendar* icu4x::Calendar::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::Calendar*>(this);
}

inline icu4x::capi::Calendar* icu4x::Calendar::AsFFI() {
    return reinterpret_cast<icu4x::capi::Calendar*>(this);
}

inline const icu4x::Calendar* icu4x::Calendar::FromFFI(const icu4x::capi::Calendar* ptr) {
    return reinterpret_cast<const icu4x::Calendar*>(ptr);
}

inline icu4x::Calendar* icu4x::Calendar::FromFFI(icu4x::capi::Calendar* ptr) {
    return reinterpret_cast<icu4x::Calendar*>(ptr);
}

inline void icu4x::Calendar::operator delete(void* ptr) {
    icu4x::capi::icu4x_Calendar_destroy_mv1(reinterpret_cast<icu4x::capi::Calendar*>(ptr));
}


#endif // ICU4X_Calendar_HPP
