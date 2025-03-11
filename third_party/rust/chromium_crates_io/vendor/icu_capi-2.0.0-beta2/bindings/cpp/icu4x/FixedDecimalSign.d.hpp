#ifndef icu4x_FixedDecimalSign_D_HPP
#define icu4x_FixedDecimalSign_D_HPP

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
    enum FixedDecimalSign {
      FixedDecimalSign_None = 0,
      FixedDecimalSign_Negative = 1,
      FixedDecimalSign_Positive = 2,
    };
    
    typedef struct FixedDecimalSign_option {union { FixedDecimalSign ok; }; bool is_ok; } FixedDecimalSign_option;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimalSign {
public:
  enum Value {
    None = 0,
    Negative = 1,
    Positive = 2,
  };

  FixedDecimalSign() = default;
  // Implicit conversions between enum and ::Value
  constexpr FixedDecimalSign(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::FixedDecimalSign AsFFI() const;
  inline static icu4x::FixedDecimalSign FromFFI(icu4x::capi::FixedDecimalSign c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_FixedDecimalSign_D_HPP
