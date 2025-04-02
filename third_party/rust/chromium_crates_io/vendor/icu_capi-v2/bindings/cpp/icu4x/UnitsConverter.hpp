#ifndef icu4x_UnitsConverter_HPP
#define icu4x_UnitsConverter_HPP

#include "UnitsConverter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    double icu4x_UnitsConverter_convert_double_mv1(const icu4x::capi::UnitsConverter* self, double value);
    
    icu4x::capi::UnitsConverter* icu4x_UnitsConverter_clone_mv1(const icu4x::capi::UnitsConverter* self);
    
    
    void icu4x_UnitsConverter_destroy_mv1(UnitsConverter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline double icu4x::UnitsConverter::convert(double value) const {
  auto result = icu4x::capi::icu4x_UnitsConverter_convert_double_mv1(this->AsFFI(),
    value);
  return result;
}

inline std::unique_ptr<icu4x::UnitsConverter> icu4x::UnitsConverter::clone() const {
  auto result = icu4x::capi::icu4x_UnitsConverter_clone_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::UnitsConverter>(icu4x::UnitsConverter::FromFFI(result));
}

inline const icu4x::capi::UnitsConverter* icu4x::UnitsConverter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::UnitsConverter*>(this);
}

inline icu4x::capi::UnitsConverter* icu4x::UnitsConverter::AsFFI() {
  return reinterpret_cast<icu4x::capi::UnitsConverter*>(this);
}

inline const icu4x::UnitsConverter* icu4x::UnitsConverter::FromFFI(const icu4x::capi::UnitsConverter* ptr) {
  return reinterpret_cast<const icu4x::UnitsConverter*>(ptr);
}

inline icu4x::UnitsConverter* icu4x::UnitsConverter::FromFFI(icu4x::capi::UnitsConverter* ptr) {
  return reinterpret_cast<icu4x::UnitsConverter*>(ptr);
}

inline void icu4x::UnitsConverter::operator delete(void* ptr) {
  icu4x::capi::icu4x_UnitsConverter_destroy_mv1(reinterpret_cast<icu4x::capi::UnitsConverter*>(ptr));
}


#endif // icu4x_UnitsConverter_HPP
