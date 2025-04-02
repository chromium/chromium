#ifndef icu4x_DateTimeLength_D_HPP
#define icu4x_DateTimeLength_D_HPP

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
    enum DateTimeLength {
      DateTimeLength_Long = 0,
      DateTimeLength_Medium = 1,
      DateTimeLength_Short = 2,
    };
    
    typedef struct DateTimeLength_option {union { DateTimeLength ok; }; bool is_ok; } DateTimeLength_option;
} // namespace capi
} // namespace

namespace icu4x {
class DateTimeLength {
public:
  enum Value {
    Long = 0,
    Medium = 1,
    Short = 2,
  };

  DateTimeLength() = default;
  // Implicit conversions between enum and ::Value
  constexpr DateTimeLength(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::DateTimeLength AsFFI() const;
  inline static icu4x::DateTimeLength FromFFI(icu4x::capi::DateTimeLength c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_DateTimeLength_D_HPP
