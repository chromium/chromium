#ifndef icu4x_WeekendContainsDay_HPP
#define icu4x_WeekendContainsDay_HPP

#include "WeekendContainsDay.d.hpp"

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


inline icu4x::capi::WeekendContainsDay icu4x::WeekendContainsDay::AsFFI() const {
  return icu4x::capi::WeekendContainsDay {
    /* .monday = */ monday,
    /* .tuesday = */ tuesday,
    /* .wednesday = */ wednesday,
    /* .thursday = */ thursday,
    /* .friday = */ friday,
    /* .saturday = */ saturday,
    /* .sunday = */ sunday,
  };
}

inline icu4x::WeekendContainsDay icu4x::WeekendContainsDay::FromFFI(icu4x::capi::WeekendContainsDay c_struct) {
  return icu4x::WeekendContainsDay {
    /* .monday = */ c_struct.monday,
    /* .tuesday = */ c_struct.tuesday,
    /* .wednesday = */ c_struct.wednesday,
    /* .thursday = */ c_struct.thursday,
    /* .friday = */ c_struct.friday,
    /* .saturday = */ c_struct.saturday,
    /* .sunday = */ c_struct.sunday,
  };
}


#endif // icu4x_WeekendContainsDay_HPP
