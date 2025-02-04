#ifndef icu4x_JoiningType_HPP
#define icu4x_JoiningType_HPP

#include "JoiningType.d.hpp"

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
    
    uint8_t icu4x_JoiningType_to_integer_mv1(icu4x::capi::JoiningType self);
    
    typedef struct icu4x_JoiningType_from_integer_mv1_result {union {icu4x::capi::JoiningType ok; }; bool is_ok;} icu4x_JoiningType_from_integer_mv1_result;
    icu4x_JoiningType_from_integer_mv1_result icu4x_JoiningType_from_integer_mv1(uint8_t other);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::JoiningType icu4x::JoiningType::AsFFI() const {
  return static_cast<icu4x::capi::JoiningType>(value);
}

inline icu4x::JoiningType icu4x::JoiningType::FromFFI(icu4x::capi::JoiningType c_enum) {
  switch (c_enum) {
    case icu4x::capi::JoiningType_NonJoining:
    case icu4x::capi::JoiningType_JoinCausing:
    case icu4x::capi::JoiningType_DualJoining:
    case icu4x::capi::JoiningType_LeftJoining:
    case icu4x::capi::JoiningType_RightJoining:
    case icu4x::capi::JoiningType_Transparent:
      return static_cast<icu4x::JoiningType::Value>(c_enum);
    default:
      abort();
  }
}

inline uint8_t icu4x::JoiningType::to_integer() {
  auto result = icu4x::capi::icu4x_JoiningType_to_integer_mv1(this->AsFFI());
  return result;
}

inline std::optional<icu4x::JoiningType> icu4x::JoiningType::from_integer(uint8_t other) {
  auto result = icu4x::capi::icu4x_JoiningType_from_integer_mv1(other);
  return result.is_ok ? std::optional<icu4x::JoiningType>(icu4x::JoiningType::FromFFI(result.ok)) : std::nullopt;
}
#endif // icu4x_JoiningType_HPP
