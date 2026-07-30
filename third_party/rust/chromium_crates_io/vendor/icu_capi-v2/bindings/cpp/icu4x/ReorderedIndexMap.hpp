#ifndef ICU4X_ReorderedIndexMap_HPP
#define ICU4X_ReorderedIndexMap_HPP

#include "ReorderedIndexMap.d.hpp"

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

    icu4x::diplomat::capi::DiplomatUsizeView icu4x_ReorderedIndexMap_as_slice_mv1(const icu4x::capi::ReorderedIndexMap* self);

    size_t icu4x_ReorderedIndexMap_len_mv1(const icu4x::capi::ReorderedIndexMap* self);

    bool icu4x_ReorderedIndexMap_is_empty_mv1(const icu4x::capi::ReorderedIndexMap* self);

    size_t icu4x_ReorderedIndexMap_get_mv1(const icu4x::capi::ReorderedIndexMap* self, size_t index);

    void icu4x_ReorderedIndexMap_destroy_mv1(ReorderedIndexMap* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::span<const size_t> icu4x::ReorderedIndexMap::as_slice() const {
    auto result = icu4x::capi::icu4x_ReorderedIndexMap_as_slice_mv1(this->AsFFI());
    return icu4x::diplomat::span<const size_t>(result.data, result.len);
}

inline size_t icu4x::ReorderedIndexMap::len() const {
    auto result = icu4x::capi::icu4x_ReorderedIndexMap_len_mv1(this->AsFFI());
    return result;
}

inline bool icu4x::ReorderedIndexMap::is_empty() const {
    auto result = icu4x::capi::icu4x_ReorderedIndexMap_is_empty_mv1(this->AsFFI());
    return result;
}

inline size_t icu4x::ReorderedIndexMap::operator[](size_t index) const {
    auto result = icu4x::capi::icu4x_ReorderedIndexMap_get_mv1(this->AsFFI(),
        index);
    return result;
}

inline const icu4x::capi::ReorderedIndexMap* icu4x::ReorderedIndexMap::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::ReorderedIndexMap*>(this);
}

inline icu4x::capi::ReorderedIndexMap* icu4x::ReorderedIndexMap::AsFFI() {
    return reinterpret_cast<icu4x::capi::ReorderedIndexMap*>(this);
}

inline const icu4x::ReorderedIndexMap* icu4x::ReorderedIndexMap::FromFFI(const icu4x::capi::ReorderedIndexMap* ptr) {
    return reinterpret_cast<const icu4x::ReorderedIndexMap*>(ptr);
}

inline icu4x::ReorderedIndexMap* icu4x::ReorderedIndexMap::FromFFI(icu4x::capi::ReorderedIndexMap* ptr) {
    return reinterpret_cast<icu4x::ReorderedIndexMap*>(ptr);
}

inline void icu4x::ReorderedIndexMap::operator delete(void* ptr) {
    icu4x::capi::icu4x_ReorderedIndexMap_destroy_mv1(reinterpret_cast<icu4x::capi::ReorderedIndexMap*>(ptr));
}


#endif // ICU4X_ReorderedIndexMap_HPP
