#ifndef icu4x_WeekOf_HPP
#define icu4x_WeekOf_HPP

#include "WeekOf.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "WeekRelativeUnit.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
} // namespace capi
} // namespace


inline icu4x::capi::WeekOf icu4x::WeekOf::AsFFI() const {
  return icu4x::capi::WeekOf {
    /* .week = */ week,
    /* .unit = */ unit.AsFFI(),
  };
}

inline icu4x::WeekOf icu4x::WeekOf::FromFFI(icu4x::capi::WeekOf c_struct) {
  return icu4x::WeekOf {
    /* .week = */ c_struct.week,
    /* .unit = */ icu4x::WeekRelativeUnit::FromFFI(c_struct.unit),
  };
}


#endif // icu4x_WeekOf_HPP
