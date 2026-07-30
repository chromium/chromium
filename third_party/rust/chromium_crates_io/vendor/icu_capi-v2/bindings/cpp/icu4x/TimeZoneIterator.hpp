#ifndef ICU4X_TimeZoneIterator_HPP
#define ICU4X_TimeZoneIterator_HPP

#include "TimeZoneIterator.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "TimeZone.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::TimeZone* icu4x_TimeZoneIterator_next_mv1(icu4x::capi::TimeZoneIterator* self);

    void icu4x_TimeZoneIterator_destroy_mv1(TimeZoneIterator* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZoneIterator::next() {
    auto result = icu4x::capi::icu4x_TimeZoneIterator_next_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline const icu4x::capi::TimeZoneIterator* icu4x::TimeZoneIterator::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::TimeZoneIterator*>(this);
}

inline icu4x::capi::TimeZoneIterator* icu4x::TimeZoneIterator::AsFFI() {
    return reinterpret_cast<icu4x::capi::TimeZoneIterator*>(this);
}

inline const icu4x::TimeZoneIterator* icu4x::TimeZoneIterator::FromFFI(const icu4x::capi::TimeZoneIterator* ptr) {
    return reinterpret_cast<const icu4x::TimeZoneIterator*>(ptr);
}

inline icu4x::TimeZoneIterator* icu4x::TimeZoneIterator::FromFFI(icu4x::capi::TimeZoneIterator* ptr) {
    return reinterpret_cast<icu4x::TimeZoneIterator*>(ptr);
}

inline void icu4x::TimeZoneIterator::operator delete(void* ptr) {
    icu4x::capi::icu4x_TimeZoneIterator_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZoneIterator*>(ptr));
}


#endif // ICU4X_TimeZoneIterator_HPP
