#ifndef icu4x_CollatorMaxVariable_HPP
#define icu4x_CollatorMaxVariable_HPP

#include "CollatorMaxVariable.d.hpp"

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

inline icu4x::capi::CollatorMaxVariable icu4x::CollatorMaxVariable::AsFFI() const {
  return static_cast<icu4x::capi::CollatorMaxVariable>(value);
}

inline icu4x::CollatorMaxVariable icu4x::CollatorMaxVariable::FromFFI(icu4x::capi::CollatorMaxVariable c_enum) {
  switch (c_enum) {
    case icu4x::capi::CollatorMaxVariable_Space:
    case icu4x::capi::CollatorMaxVariable_Punctuation:
    case icu4x::capi::CollatorMaxVariable_Symbol:
    case icu4x::capi::CollatorMaxVariable_Currency:
      return static_cast<icu4x::CollatorMaxVariable::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_CollatorMaxVariable_HPP
