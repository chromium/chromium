#ifndef ICU4X_WordBreakIteratorLatin1_HPP
#define ICU4X_WordBreakIteratorLatin1_HPP

#include "WordBreakIteratorLatin1.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "SegmenterWordType.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    int32_t icu4x_WordBreakIteratorLatin1_next_mv1(icu4x::capi::WordBreakIteratorLatin1* self);

    icu4x::capi::SegmenterWordType icu4x_WordBreakIteratorLatin1_word_type_mv1(const icu4x::capi::WordBreakIteratorLatin1* self);

    bool icu4x_WordBreakIteratorLatin1_is_word_like_mv1(const icu4x::capi::WordBreakIteratorLatin1* self);

    void icu4x_WordBreakIteratorLatin1_destroy_mv1(WordBreakIteratorLatin1* self);

    } // extern "C"
} // namespace capi
} // namespace

inline int32_t icu4x::WordBreakIteratorLatin1::next() {
    auto result = icu4x::capi::icu4x_WordBreakIteratorLatin1_next_mv1(this->AsFFI());
    return result;
}

inline icu4x::SegmenterWordType icu4x::WordBreakIteratorLatin1::word_type() const {
    auto result = icu4x::capi::icu4x_WordBreakIteratorLatin1_word_type_mv1(this->AsFFI());
    return icu4x::SegmenterWordType::FromFFI(result);
}

inline bool icu4x::WordBreakIteratorLatin1::is_word_like() const {
    auto result = icu4x::capi::icu4x_WordBreakIteratorLatin1_is_word_like_mv1(this->AsFFI());
    return result;
}

inline const icu4x::capi::WordBreakIteratorLatin1* icu4x::WordBreakIteratorLatin1::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::WordBreakIteratorLatin1*>(this);
}

inline icu4x::capi::WordBreakIteratorLatin1* icu4x::WordBreakIteratorLatin1::AsFFI() {
    return reinterpret_cast<icu4x::capi::WordBreakIteratorLatin1*>(this);
}

inline const icu4x::WordBreakIteratorLatin1* icu4x::WordBreakIteratorLatin1::FromFFI(const icu4x::capi::WordBreakIteratorLatin1* ptr) {
    return reinterpret_cast<const icu4x::WordBreakIteratorLatin1*>(ptr);
}

inline icu4x::WordBreakIteratorLatin1* icu4x::WordBreakIteratorLatin1::FromFFI(icu4x::capi::WordBreakIteratorLatin1* ptr) {
    return reinterpret_cast<icu4x::WordBreakIteratorLatin1*>(ptr);
}

inline void icu4x::WordBreakIteratorLatin1::operator delete(void* ptr) {
    icu4x::capi::icu4x_WordBreakIteratorLatin1_destroy_mv1(reinterpret_cast<icu4x::capi::WordBreakIteratorLatin1*>(ptr));
}


#endif // ICU4X_WordBreakIteratorLatin1_HPP
