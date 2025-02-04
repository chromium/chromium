#ifndef icu4x_IsoWeekday_D_HPP
#define icu4x_IsoWeekday_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum IsoWeekday {
      IsoWeekday_Monday = 1,
      IsoWeekday_Tuesday = 2,
      IsoWeekday_Wednesday = 3,
      IsoWeekday_Thursday = 4,
      IsoWeekday_Friday = 5,
      IsoWeekday_Saturday = 6,
      IsoWeekday_Sunday = 7,
    };
    
    typedef struct IsoWeekday_option {union { IsoWeekday ok; }; bool is_ok; } IsoWeekday_option;
} // namespace capi
} // namespace

namespace icu4x {
class IsoWeekday {
public:
  enum Value {
    Monday = 1,
    Tuesday = 2,
    Wednesday = 3,
    Thursday = 4,
    Friday = 5,
    Saturday = 6,
    Sunday = 7,
  };

  IsoWeekday() = default;
  // Implicit conversions between enum and ::Value
  constexpr IsoWeekday(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::IsoWeekday AsFFI() const;
  inline static icu4x::IsoWeekday FromFFI(icu4x::capi::IsoWeekday c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_IsoWeekday_D_HPP
