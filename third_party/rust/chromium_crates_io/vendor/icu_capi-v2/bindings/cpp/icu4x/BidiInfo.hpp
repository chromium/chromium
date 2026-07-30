#ifndef ICU4X_BidiInfo_HPP
#define ICU4X_BidiInfo_HPP

#include "BidiInfo.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "BidiParagraph.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    size_t icu4x_BidiInfo_paragraph_count_mv1(const icu4x::capi::BidiInfo* self);

    icu4x::capi::BidiParagraph* icu4x_BidiInfo_paragraph_at_mv1(const icu4x::capi::BidiInfo* self, size_t n);

    size_t icu4x_BidiInfo_size_mv1(const icu4x::capi::BidiInfo* self);

    uint8_t icu4x_BidiInfo_level_at_mv1(const icu4x::capi::BidiInfo* self, size_t pos);

    void icu4x_BidiInfo_destroy_mv1(BidiInfo* self);

    } // extern "C"
} // namespace capi
} // namespace

inline size_t icu4x::BidiInfo::paragraph_count() const {
    auto result = icu4x::capi::icu4x_BidiInfo_paragraph_count_mv1(this->AsFFI());
    return result;
}

inline std::unique_ptr<icu4x::BidiParagraph> icu4x::BidiInfo::paragraph_at(size_t n) const {
    auto result = icu4x::capi::icu4x_BidiInfo_paragraph_at_mv1(this->AsFFI(),
        n);
    return std::unique_ptr<icu4x::BidiParagraph>(icu4x::BidiParagraph::FromFFI(result));
}

inline size_t icu4x::BidiInfo::size() const {
    auto result = icu4x::capi::icu4x_BidiInfo_size_mv1(this->AsFFI());
    return result;
}

inline uint8_t icu4x::BidiInfo::level_at(size_t pos) const {
    auto result = icu4x::capi::icu4x_BidiInfo_level_at_mv1(this->AsFFI(),
        pos);
    return result;
}

inline const icu4x::capi::BidiInfo* icu4x::BidiInfo::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::BidiInfo*>(this);
}

inline icu4x::capi::BidiInfo* icu4x::BidiInfo::AsFFI() {
    return reinterpret_cast<icu4x::capi::BidiInfo*>(this);
}

inline const icu4x::BidiInfo* icu4x::BidiInfo::FromFFI(const icu4x::capi::BidiInfo* ptr) {
    return reinterpret_cast<const icu4x::BidiInfo*>(ptr);
}

inline icu4x::BidiInfo* icu4x::BidiInfo::FromFFI(icu4x::capi::BidiInfo* ptr) {
    return reinterpret_cast<icu4x::BidiInfo*>(ptr);
}

inline void icu4x::BidiInfo::operator delete(void* ptr) {
    icu4x::capi::icu4x_BidiInfo_destroy_mv1(reinterpret_cast<icu4x::capi::BidiInfo*>(ptr));
}


#endif // ICU4X_BidiInfo_HPP
