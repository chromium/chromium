#ifndef icu4x_WeekOf_D_HPP
#define icu4x_WeekOf_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "WeekRelativeUnit.d.hpp"

namespace icu4x {
class WeekRelativeUnit;
}


namespace icu4x {
namespace capi {
    struct WeekOf {
      uint8_t week;
      icu4x::capi::WeekRelativeUnit unit;
    };
    
    typedef struct WeekOf_option {union { WeekOf ok; }; bool is_ok; } WeekOf_option;
} // namespace capi
} // namespace


namespace icu4x {
struct WeekOf {
  uint8_t week;
  icu4x::WeekRelativeUnit unit;

  inline icu4x::capi::WeekOf AsFFI() const;
  inline static icu4x::WeekOf FromFFI(icu4x::capi::WeekOf c_struct);
};

} // namespace
#endif // icu4x_WeekOf_D_HPP
