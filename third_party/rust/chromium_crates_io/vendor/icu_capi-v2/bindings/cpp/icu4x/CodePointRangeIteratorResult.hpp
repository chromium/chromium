#ifndef ICU4X_CodePointRangeIteratorResult_HPP
#define ICU4X_CodePointRangeIteratorResult_HPP

#include "CodePointRangeIteratorResult.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::CodePointRangeIteratorResult icu4x::CodePointRangeIteratorResult::AsFFI() const {
    return icu4x::capi::CodePointRangeIteratorResult {
        /* .start = */ start,
        /* .end = */ end,
        /* .done = */ done,
    };
}

inline icu4x::CodePointRangeIteratorResult icu4x::CodePointRangeIteratorResult::FromFFI(icu4x::capi::CodePointRangeIteratorResult c_struct) {
    return icu4x::CodePointRangeIteratorResult {
        /* .start = */ c_struct.start,
        /* .end = */ c_struct.end,
        /* .done = */ c_struct.done,
    };
}


#endif // ICU4X_CodePointRangeIteratorResult_HPP
