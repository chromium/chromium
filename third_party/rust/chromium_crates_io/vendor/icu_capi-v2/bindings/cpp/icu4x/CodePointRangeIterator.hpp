#ifndef ICU4X_CodePointRangeIterator_HPP
#define ICU4X_CodePointRangeIterator_HPP

#include "CodePointRangeIterator.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointRangeIteratorResult.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::CodePointRangeIteratorResult icu4x_CodePointRangeIterator_next_mv1(icu4x::capi::CodePointRangeIterator* self);

    void icu4x_CodePointRangeIterator_destroy_mv1(CodePointRangeIterator* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::CodePointRangeIteratorResult icu4x::CodePointRangeIterator::next() {
    auto result = icu4x::capi::icu4x_CodePointRangeIterator_next_mv1(this->AsFFI());
    return icu4x::CodePointRangeIteratorResult::FromFFI(result);
}

inline const icu4x::capi::CodePointRangeIterator* icu4x::CodePointRangeIterator::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::CodePointRangeIterator*>(this);
}

inline icu4x::capi::CodePointRangeIterator* icu4x::CodePointRangeIterator::AsFFI() {
    return reinterpret_cast<icu4x::capi::CodePointRangeIterator*>(this);
}

inline const icu4x::CodePointRangeIterator* icu4x::CodePointRangeIterator::FromFFI(const icu4x::capi::CodePointRangeIterator* ptr) {
    return reinterpret_cast<const icu4x::CodePointRangeIterator*>(ptr);
}

inline icu4x::CodePointRangeIterator* icu4x::CodePointRangeIterator::FromFFI(icu4x::capi::CodePointRangeIterator* ptr) {
    return reinterpret_cast<icu4x::CodePointRangeIterator*>(ptr);
}

inline void icu4x::CodePointRangeIterator::operator delete(void* ptr) {
    icu4x::capi::icu4x_CodePointRangeIterator_destroy_mv1(reinterpret_cast<icu4x::capi::CodePointRangeIterator*>(ptr));
}


#endif // ICU4X_CodePointRangeIterator_HPP
