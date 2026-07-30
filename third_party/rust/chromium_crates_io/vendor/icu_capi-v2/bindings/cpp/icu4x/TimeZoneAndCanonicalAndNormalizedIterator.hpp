#ifndef ICU4X_TimeZoneAndCanonicalAndNormalizedIterator_HPP
#define ICU4X_TimeZoneAndCanonicalAndNormalizedIterator_HPP

#include "TimeZoneAndCanonicalAndNormalizedIterator.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "TimeZoneAndCanonicalAndNormalized.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_TimeZoneAndCanonicalAndNormalizedIterator_next_mv1_result {union {icu4x::capi::TimeZoneAndCanonicalAndNormalized ok; }; bool is_ok;} icu4x_TimeZoneAndCanonicalAndNormalizedIterator_next_mv1_result;
    icu4x_TimeZoneAndCanonicalAndNormalizedIterator_next_mv1_result icu4x_TimeZoneAndCanonicalAndNormalizedIterator_next_mv1(icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* self);

    void icu4x_TimeZoneAndCanonicalAndNormalizedIterator_destroy_mv1(TimeZoneAndCanonicalAndNormalizedIterator* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::optional<icu4x::TimeZoneAndCanonicalAndNormalized> icu4x::TimeZoneAndCanonicalAndNormalizedIterator::next() {
    auto result = icu4x::capi::icu4x_TimeZoneAndCanonicalAndNormalizedIterator_next_mv1(this->AsFFI());
    return result.is_ok ? std::optional<icu4x::TimeZoneAndCanonicalAndNormalized>(icu4x::TimeZoneAndCanonicalAndNormalized::FromFFI(result.ok)) : std::nullopt;
}

inline const icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* icu4x::TimeZoneAndCanonicalAndNormalizedIterator::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator*>(this);
}

inline icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* icu4x::TimeZoneAndCanonicalAndNormalizedIterator::AsFFI() {
    return reinterpret_cast<icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator*>(this);
}

inline const icu4x::TimeZoneAndCanonicalAndNormalizedIterator* icu4x::TimeZoneAndCanonicalAndNormalizedIterator::FromFFI(const icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* ptr) {
    return reinterpret_cast<const icu4x::TimeZoneAndCanonicalAndNormalizedIterator*>(ptr);
}

inline icu4x::TimeZoneAndCanonicalAndNormalizedIterator* icu4x::TimeZoneAndCanonicalAndNormalizedIterator::FromFFI(icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* ptr) {
    return reinterpret_cast<icu4x::TimeZoneAndCanonicalAndNormalizedIterator*>(ptr);
}

inline void icu4x::TimeZoneAndCanonicalAndNormalizedIterator::operator delete(void* ptr) {
    icu4x::capi::icu4x_TimeZoneAndCanonicalAndNormalizedIterator_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator*>(ptr));
}


#endif // ICU4X_TimeZoneAndCanonicalAndNormalizedIterator_HPP
