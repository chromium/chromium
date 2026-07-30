#ifndef ICU4X_WeekdaySetIterator_HPP
#define ICU4X_WeekdaySetIterator_HPP

#include "WeekdaySetIterator.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Weekday.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_WeekdaySetIterator_next_mv1_result {union {icu4x::capi::Weekday ok; }; bool is_ok;} icu4x_WeekdaySetIterator_next_mv1_result;
    icu4x_WeekdaySetIterator_next_mv1_result icu4x_WeekdaySetIterator_next_mv1(icu4x::capi::WeekdaySetIterator* self);

    void icu4x_WeekdaySetIterator_destroy_mv1(WeekdaySetIterator* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::optional<icu4x::Weekday> icu4x::WeekdaySetIterator::next() {
    auto result = icu4x::capi::icu4x_WeekdaySetIterator_next_mv1(this->AsFFI());
    return result.is_ok ? std::optional<icu4x::Weekday>(icu4x::Weekday::FromFFI(result.ok)) : std::nullopt;
}

inline const icu4x::capi::WeekdaySetIterator* icu4x::WeekdaySetIterator::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::WeekdaySetIterator*>(this);
}

inline icu4x::capi::WeekdaySetIterator* icu4x::WeekdaySetIterator::AsFFI() {
    return reinterpret_cast<icu4x::capi::WeekdaySetIterator*>(this);
}

inline const icu4x::WeekdaySetIterator* icu4x::WeekdaySetIterator::FromFFI(const icu4x::capi::WeekdaySetIterator* ptr) {
    return reinterpret_cast<const icu4x::WeekdaySetIterator*>(ptr);
}

inline icu4x::WeekdaySetIterator* icu4x::WeekdaySetIterator::FromFFI(icu4x::capi::WeekdaySetIterator* ptr) {
    return reinterpret_cast<icu4x::WeekdaySetIterator*>(ptr);
}

inline void icu4x::WeekdaySetIterator::operator delete(void* ptr) {
    icu4x::capi::icu4x_WeekdaySetIterator_destroy_mv1(reinterpret_cast<icu4x::capi::WeekdaySetIterator*>(ptr));
}


#endif // ICU4X_WeekdaySetIterator_HPP
