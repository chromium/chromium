#ifndef icu4x_BidiDirection_D_HPP
#define icu4x_BidiDirection_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum BidiDirection {
      BidiDirection_Ltr = 0,
      BidiDirection_Rtl = 1,
      BidiDirection_Mixed = 2,
    };
    
    typedef struct BidiDirection_option {union { BidiDirection ok; }; bool is_ok; } BidiDirection_option;
} // namespace capi
} // namespace

namespace icu4x {
class BidiDirection {
public:
  enum Value {
    Ltr = 0,
    Rtl = 1,
    Mixed = 2,
  };

  BidiDirection() = default;
  // Implicit conversions between enum and ::Value
  constexpr BidiDirection(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::BidiDirection AsFFI() const;
  inline static icu4x::BidiDirection FromFFI(icu4x::capi::BidiDirection c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_BidiDirection_D_HPP
