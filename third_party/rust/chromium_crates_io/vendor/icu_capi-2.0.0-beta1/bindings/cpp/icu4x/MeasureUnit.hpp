#ifndef icu4x_MeasureUnit_HPP
#define icu4x_MeasureUnit_HPP

#include "MeasureUnit.d.hpp"

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
    
    
    void icu4x_MeasureUnit_destroy_mv1(MeasureUnit* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline const icu4x::capi::MeasureUnit* icu4x::MeasureUnit::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::MeasureUnit*>(this);
}

inline icu4x::capi::MeasureUnit* icu4x::MeasureUnit::AsFFI() {
  return reinterpret_cast<icu4x::capi::MeasureUnit*>(this);
}

inline const icu4x::MeasureUnit* icu4x::MeasureUnit::FromFFI(const icu4x::capi::MeasureUnit* ptr) {
  return reinterpret_cast<const icu4x::MeasureUnit*>(ptr);
}

inline icu4x::MeasureUnit* icu4x::MeasureUnit::FromFFI(icu4x::capi::MeasureUnit* ptr) {
  return reinterpret_cast<icu4x::MeasureUnit*>(ptr);
}

inline void icu4x::MeasureUnit::operator delete(void* ptr) {
  icu4x::capi::icu4x_MeasureUnit_destroy_mv1(reinterpret_cast<icu4x::capi::MeasureUnit*>(ptr));
}


#endif // icu4x_MeasureUnit_HPP
