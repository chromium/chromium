#ifndef icu4x_DisplayNamesStyle_HPP
#define icu4x_DisplayNamesStyle_HPP

#include "DisplayNamesStyle.d.hpp"

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

inline icu4x::capi::DisplayNamesStyle icu4x::DisplayNamesStyle::AsFFI() const {
  return static_cast<icu4x::capi::DisplayNamesStyle>(value);
}

inline icu4x::DisplayNamesStyle icu4x::DisplayNamesStyle::FromFFI(icu4x::capi::DisplayNamesStyle c_enum) {
  switch (c_enum) {
    case icu4x::capi::DisplayNamesStyle_Narrow:
    case icu4x::capi::DisplayNamesStyle_Short:
    case icu4x::capi::DisplayNamesStyle_Long:
    case icu4x::capi::DisplayNamesStyle_Menu:
      return static_cast<icu4x::DisplayNamesStyle::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_DisplayNamesStyle_HPP
