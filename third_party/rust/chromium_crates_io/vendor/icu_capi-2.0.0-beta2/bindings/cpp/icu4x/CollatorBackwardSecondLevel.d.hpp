#ifndef icu4x_CollatorBackwardSecondLevel_D_HPP
#define icu4x_CollatorBackwardSecondLevel_D_HPP

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
    enum CollatorBackwardSecondLevel {
      CollatorBackwardSecondLevel_Off = 0,
      CollatorBackwardSecondLevel_On = 1,
    };
    
    typedef struct CollatorBackwardSecondLevel_option {union { CollatorBackwardSecondLevel ok; }; bool is_ok; } CollatorBackwardSecondLevel_option;
} // namespace capi
} // namespace

namespace icu4x {
class CollatorBackwardSecondLevel {
public:
  enum Value {
    Off = 0,
    On = 1,
  };

  CollatorBackwardSecondLevel() = default;
  // Implicit conversions between enum and ::Value
  constexpr CollatorBackwardSecondLevel(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::CollatorBackwardSecondLevel AsFFI() const;
  inline static icu4x::CollatorBackwardSecondLevel FromFFI(icu4x::capi::CollatorBackwardSecondLevel c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_CollatorBackwardSecondLevel_D_HPP
