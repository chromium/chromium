#ifndef icu4x_UtcOffsets_HPP
#define icu4x_UtcOffsets_HPP

#include "UtcOffsets.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "UtcOffset.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
} // namespace capi
} // namespace


inline icu4x::capi::UtcOffsets icu4x::UtcOffsets::AsFFI() const {
  return icu4x::capi::UtcOffsets {
    /* .standard = */ standard->AsFFI(),
    /* .daylight = */ daylight ? daylight->AsFFI() : nullptr,
  };
}

inline icu4x::UtcOffsets icu4x::UtcOffsets::FromFFI(icu4x::capi::UtcOffsets c_struct) {
  return icu4x::UtcOffsets {
    /* .standard = */ std::unique_ptr<icu4x::UtcOffset>(icu4x::UtcOffset::FromFFI(c_struct.standard)),
    /* .daylight = */ std::unique_ptr<icu4x::UtcOffset>(icu4x::UtcOffset::FromFFI(c_struct.daylight)),
  };
}


#endif // icu4x_UtcOffsets_HPP
