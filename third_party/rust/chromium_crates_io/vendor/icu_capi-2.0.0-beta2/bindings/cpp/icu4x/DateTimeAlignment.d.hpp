#ifndef icu4x_DateTimeAlignment_D_HPP
#define icu4x_DateTimeAlignment_D_HPP

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
    enum DateTimeAlignment {
      DateTimeAlignment_Auto = 0,
      DateTimeAlignment_Column = 1,
    };
    
    typedef struct DateTimeAlignment_option {union { DateTimeAlignment ok; }; bool is_ok; } DateTimeAlignment_option;
} // namespace capi
} // namespace

namespace icu4x {
class DateTimeAlignment {
public:
  enum Value {
    Auto = 0,
    Column = 1,
  };

  DateTimeAlignment() = default;
  // Implicit conversions between enum and ::Value
  constexpr DateTimeAlignment(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::DateTimeAlignment AsFFI() const;
  inline static icu4x::DateTimeAlignment FromFFI(icu4x::capi::DateTimeAlignment c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_DateTimeAlignment_D_HPP
