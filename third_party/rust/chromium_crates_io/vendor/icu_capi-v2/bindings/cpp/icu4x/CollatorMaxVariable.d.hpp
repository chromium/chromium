#ifndef icu4x_CollatorMaxVariable_D_HPP
#define icu4x_CollatorMaxVariable_D_HPP

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
    enum CollatorMaxVariable {
      CollatorMaxVariable_Space = 0,
      CollatorMaxVariable_Punctuation = 1,
      CollatorMaxVariable_Symbol = 2,
      CollatorMaxVariable_Currency = 3,
    };
    
    typedef struct CollatorMaxVariable_option {union { CollatorMaxVariable ok; }; bool is_ok; } CollatorMaxVariable_option;
} // namespace capi
} // namespace

namespace icu4x {
class CollatorMaxVariable {
public:
  enum Value {
    Space = 0,
    Punctuation = 1,
    Symbol = 2,
    Currency = 3,
  };

  CollatorMaxVariable() = default;
  // Implicit conversions between enum and ::Value
  constexpr CollatorMaxVariable(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::CollatorMaxVariable AsFFI() const;
  inline static icu4x::CollatorMaxVariable FromFFI(icu4x::capi::CollatorMaxVariable c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_CollatorMaxVariable_D_HPP
