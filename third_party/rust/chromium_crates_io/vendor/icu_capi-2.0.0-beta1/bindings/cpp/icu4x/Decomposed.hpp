#ifndef icu4x_Decomposed_HPP
#define icu4x_Decomposed_HPP

#include "Decomposed.d.hpp"

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


inline icu4x::capi::Decomposed icu4x::Decomposed::AsFFI() const {
  return icu4x::capi::Decomposed {
    /* .first = */ first,
    /* .second = */ second,
  };
}

inline icu4x::Decomposed icu4x::Decomposed::FromFFI(icu4x::capi::Decomposed c_struct) {
  return icu4x::Decomposed {
    /* .first = */ c_struct.first,
    /* .second = */ c_struct.second,
  };
}


#endif // icu4x_Decomposed_HPP
