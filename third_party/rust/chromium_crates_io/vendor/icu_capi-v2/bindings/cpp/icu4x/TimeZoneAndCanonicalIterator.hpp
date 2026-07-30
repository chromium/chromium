#ifndef ICU4X_TimeZoneAndCanonicalIterator_HPP
#define ICU4X_TimeZoneAndCanonicalIterator_HPP

#include "TimeZoneAndCanonicalIterator.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "TimeZoneAndCanonical.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_TimeZoneAndCanonicalIterator_next_mv1_result {union {icu4x::capi::TimeZoneAndCanonical ok; }; bool is_ok;} icu4x_TimeZoneAndCanonicalIterator_next_mv1_result;
    icu4x_TimeZoneAndCanonicalIterator_next_mv1_result icu4x_TimeZoneAndCanonicalIterator_next_mv1(icu4x::capi::TimeZoneAndCanonicalIterator* self);

    void icu4x_TimeZoneAndCanonicalIterator_destroy_mv1(TimeZoneAndCanonicalIterator* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::optional<icu4x::TimeZoneAndCanonical> icu4x::TimeZoneAndCanonicalIterator::next() {
    auto result = icu4x::capi::icu4x_TimeZoneAndCanonicalIterator_next_mv1(this->AsFFI());
    return result.is_ok ? std::optional<icu4x::TimeZoneAndCanonical>(icu4x::TimeZoneAndCanonical::FromFFI(result.ok)) : std::nullopt;
}

inline const icu4x::capi::TimeZoneAndCanonicalIterator* icu4x::TimeZoneAndCanonicalIterator::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::TimeZoneAndCanonicalIterator*>(this);
}

inline icu4x::capi::TimeZoneAndCanonicalIterator* icu4x::TimeZoneAndCanonicalIterator::AsFFI() {
    return reinterpret_cast<icu4x::capi::TimeZoneAndCanonicalIterator*>(this);
}

inline const icu4x::TimeZoneAndCanonicalIterator* icu4x::TimeZoneAndCanonicalIterator::FromFFI(const icu4x::capi::TimeZoneAndCanonicalIterator* ptr) {
    return reinterpret_cast<const icu4x::TimeZoneAndCanonicalIterator*>(ptr);
}

inline icu4x::TimeZoneAndCanonicalIterator* icu4x::TimeZoneAndCanonicalIterator::FromFFI(icu4x::capi::TimeZoneAndCanonicalIterator* ptr) {
    return reinterpret_cast<icu4x::TimeZoneAndCanonicalIterator*>(ptr);
}

inline void icu4x::TimeZoneAndCanonicalIterator::operator delete(void* ptr) {
    icu4x::capi::icu4x_TimeZoneAndCanonicalIterator_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZoneAndCanonicalIterator*>(ptr));
}


#endif // ICU4X_TimeZoneAndCanonicalIterator_HPP
