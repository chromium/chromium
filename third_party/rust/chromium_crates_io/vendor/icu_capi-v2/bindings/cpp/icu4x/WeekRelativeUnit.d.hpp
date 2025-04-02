#ifndef icu4x_WeekRelativeUnit_D_HPP
#define icu4x_WeekRelativeUnit_D_HPP

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
    enum WeekRelativeUnit {
      WeekRelativeUnit_Previous = 0,
      WeekRelativeUnit_Current = 1,
      WeekRelativeUnit_Next = 2,
    };
    
    typedef struct WeekRelativeUnit_option {union { WeekRelativeUnit ok; }; bool is_ok; } WeekRelativeUnit_option;
} // namespace capi
} // namespace

namespace icu4x {
class WeekRelativeUnit {
public:
  enum Value {
    Previous = 0,
    Current = 1,
    Next = 2,
  };

  WeekRelativeUnit() = default;
  // Implicit conversions between enum and ::Value
  constexpr WeekRelativeUnit(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::WeekRelativeUnit AsFFI() const;
  inline static icu4x::WeekRelativeUnit FromFFI(icu4x::capi::WeekRelativeUnit c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_WeekRelativeUnit_D_HPP
