#ifndef ICU4X_GraphemeClusterBreakIteratorLatin1_HPP
#define ICU4X_GraphemeClusterBreakIteratorLatin1_HPP

#include "GraphemeClusterBreakIteratorLatin1.d.hpp"

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
    extern "C" {

    int32_t icu4x_GraphemeClusterBreakIteratorLatin1_next_mv1(icu4x::capi::GraphemeClusterBreakIteratorLatin1* self);

    void icu4x_GraphemeClusterBreakIteratorLatin1_destroy_mv1(GraphemeClusterBreakIteratorLatin1* self);

    } // extern "C"
} // namespace capi
} // namespace

inline int32_t icu4x::GraphemeClusterBreakIteratorLatin1::next() {
    auto result = icu4x::capi::icu4x_GraphemeClusterBreakIteratorLatin1_next_mv1(this->AsFFI());
    return result;
}

inline const icu4x::capi::GraphemeClusterBreakIteratorLatin1* icu4x::GraphemeClusterBreakIteratorLatin1::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::GraphemeClusterBreakIteratorLatin1*>(this);
}

inline icu4x::capi::GraphemeClusterBreakIteratorLatin1* icu4x::GraphemeClusterBreakIteratorLatin1::AsFFI() {
    return reinterpret_cast<icu4x::capi::GraphemeClusterBreakIteratorLatin1*>(this);
}

inline const icu4x::GraphemeClusterBreakIteratorLatin1* icu4x::GraphemeClusterBreakIteratorLatin1::FromFFI(const icu4x::capi::GraphemeClusterBreakIteratorLatin1* ptr) {
    return reinterpret_cast<const icu4x::GraphemeClusterBreakIteratorLatin1*>(ptr);
}

inline icu4x::GraphemeClusterBreakIteratorLatin1* icu4x::GraphemeClusterBreakIteratorLatin1::FromFFI(icu4x::capi::GraphemeClusterBreakIteratorLatin1* ptr) {
    return reinterpret_cast<icu4x::GraphemeClusterBreakIteratorLatin1*>(ptr);
}

inline void icu4x::GraphemeClusterBreakIteratorLatin1::operator delete(void* ptr) {
    icu4x::capi::icu4x_GraphemeClusterBreakIteratorLatin1_destroy_mv1(reinterpret_cast<icu4x::capi::GraphemeClusterBreakIteratorLatin1*>(ptr));
}


#endif // ICU4X_GraphemeClusterBreakIteratorLatin1_HPP
