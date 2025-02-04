#ifndef icu4x_PluralCategories_HPP
#define icu4x_PluralCategories_HPP

#include "PluralCategories.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
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


#endif // icu4x_PluralCategories_HPP
