#ifndef icu4x_TrailingCase_D_HPP
#define icu4x_TrailingCase_D_HPP

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
    enum TrailingCase {
      TrailingCase_Lower = 0,
      TrailingCase_Unchanged = 1,
    };
    
    typedef struct TrailingCase_option {union { TrailingCase ok; }; bool is_ok; } TrailingCase_option;
} // namespace capi
} // namespace

namespace icu4x {
class TrailingCase {
public:
  enum Value {
    Lower = 0,
    Unchanged = 1,
  };

  TrailingCase() = default;
  // Implicit conversions between enum and ::Value
  constexpr TrailingCase(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::TrailingCase AsFFI() const;
  inline static icu4x::TrailingCase FromFFI(icu4x::capi::TrailingCase c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_TrailingCase_D_HPP
