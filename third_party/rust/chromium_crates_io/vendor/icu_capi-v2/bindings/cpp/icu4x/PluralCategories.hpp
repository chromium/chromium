#ifndef ICU4X_PluralCategories_HPP
#define ICU4X_PluralCategories_HPP

#include "PluralCategories.d.hpp"

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


inline icu4x::capi::PluralCategories icu4x::PluralCategories::AsFFI() const {
    return icu4x::capi::PluralCategories {
        /* .zero = */ zero,
        /* .one = */ one,
        /* .two = */ two,
        /* .few = */ few,
        /* .many = */ many,
        /* .other = */ other,
    };
}

inline icu4x::PluralCategories icu4x::PluralCategories::FromFFI(icu4x::capi::PluralCategories c_struct) {
    return icu4x::PluralCategories {
        /* .zero = */ c_struct.zero,
        /* .one = */ c_struct.one,
        /* .two = */ c_struct.two,
        /* .few = */ c_struct.few,
        /* .many = */ c_struct.many,
        /* .other = */ c_struct.other,
    };
}


#endif // ICU4X_PluralCategories_HPP
