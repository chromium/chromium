#ifndef ICU4X_SentenceBreakIteratorUtf8_HPP
#define ICU4X_SentenceBreakIteratorUtf8_HPP

#include "SentenceBreakIteratorUtf8.d.hpp"

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

    int32_t icu4x_SentenceBreakIteratorUtf8_next_mv1(icu4x::capi::SentenceBreakIteratorUtf8* self);

    void icu4x_SentenceBreakIteratorUtf8_destroy_mv1(SentenceBreakIteratorUtf8* self);

    } // extern "C"
} // namespace capi
} // namespace

inline int32_t icu4x::SentenceBreakIteratorUtf8::next() {
    auto result = icu4x::capi::icu4x_SentenceBreakIteratorUtf8_next_mv1(this->AsFFI());
    return result;
}

inline const icu4x::capi::SentenceBreakIteratorUtf8* icu4x::SentenceBreakIteratorUtf8::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::SentenceBreakIteratorUtf8*>(this);
}

inline icu4x::capi::SentenceBreakIteratorUtf8* icu4x::SentenceBreakIteratorUtf8::AsFFI() {
    return reinterpret_cast<icu4x::capi::SentenceBreakIteratorUtf8*>(this);
}

inline const icu4x::SentenceBreakIteratorUtf8* icu4x::SentenceBreakIteratorUtf8::FromFFI(const icu4x::capi::SentenceBreakIteratorUtf8* ptr) {
    return reinterpret_cast<const icu4x::SentenceBreakIteratorUtf8*>(ptr);
}

inline icu4x::SentenceBreakIteratorUtf8* icu4x::SentenceBreakIteratorUtf8::FromFFI(icu4x::capi::SentenceBreakIteratorUtf8* ptr) {
    return reinterpret_cast<icu4x::SentenceBreakIteratorUtf8*>(ptr);
}

inline void icu4x::SentenceBreakIteratorUtf8::operator delete(void* ptr) {
    icu4x::capi::icu4x_SentenceBreakIteratorUtf8_destroy_mv1(reinterpret_cast<icu4x::capi::SentenceBreakIteratorUtf8*>(ptr));
}


#endif // ICU4X_SentenceBreakIteratorUtf8_HPP
