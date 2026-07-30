#ifndef ICU4X_WordBreakIteratorUtf8_HPP
#define ICU4X_WordBreakIteratorUtf8_HPP

#include "WordBreakIteratorUtf8.d.hpp"

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

    int32_t icu4x_WordBreakIteratorUtf8_next_mv1(icu4x::capi::WordBreakIteratorUtf8* self);

    icu4x::capi::SegmenterWordType icu4x_WordBreakIteratorUtf8_word_type_mv1(const icu4x::capi::WordBreakIteratorUtf8* self);

    bool icu4x_WordBreakIteratorUtf8_is_word_like_mv1(const icu4x::capi::WordBreakIteratorUtf8* self);

    void icu4x_WordBreakIteratorUtf8_destroy_mv1(WordBreakIteratorUtf8* self);

    } // extern "C"
} // namespace capi
} // namespace

inline int32_t icu4x::WordBreakIteratorUtf8::next() {
    auto result = icu4x::capi::icu4x_WordBreakIteratorUtf8_next_mv1(this->AsFFI());
    return result;
}

inline icu4x::SegmenterWordType icu4x::WordBreakIteratorUtf8::word_type() const {
    auto result = icu4x::capi::icu4x_WordBreakIteratorUtf8_word_type_mv1(this->AsFFI());
    return icu4x::SegmenterWordType::FromFFI(result);
}

inline bool icu4x::WordBreakIteratorUtf8::is_word_like() const {
    auto result = icu4x::capi::icu4x_WordBreakIteratorUtf8_is_word_like_mv1(this->AsFFI());
    return result;
}

inline const icu4x::capi::WordBreakIteratorUtf8* icu4x::WordBreakIteratorUtf8::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::WordBreakIteratorUtf8*>(this);
}

inline icu4x::capi::WordBreakIteratorUtf8* icu4x::WordBreakIteratorUtf8::AsFFI() {
    return reinterpret_cast<icu4x::capi::WordBreakIteratorUtf8*>(this);
}

inline const icu4x::WordBreakIteratorUtf8* icu4x::WordBreakIteratorUtf8::FromFFI(const icu4x::capi::WordBreakIteratorUtf8* ptr) {
    return reinterpret_cast<const icu4x::WordBreakIteratorUtf8*>(ptr);
}

inline icu4x::WordBreakIteratorUtf8* icu4x::WordBreakIteratorUtf8::FromFFI(icu4x::capi::WordBreakIteratorUtf8* ptr) {
    return reinterpret_cast<icu4x::WordBreakIteratorUtf8*>(ptr);
}

inline void icu4x::WordBreakIteratorUtf8::operator delete(void* ptr) {
    icu4x::capi::icu4x_WordBreakIteratorUtf8_destroy_mv1(reinterpret_cast<icu4x::capi::WordBreakIteratorUtf8*>(ptr));
}


#endif // ICU4X_WordBreakIteratorUtf8_HPP
