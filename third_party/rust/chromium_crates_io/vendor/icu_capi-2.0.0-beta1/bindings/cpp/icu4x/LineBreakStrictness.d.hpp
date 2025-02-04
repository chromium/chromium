#ifndef icu4x_LineBreakStrictness_D_HPP
#define icu4x_LineBreakStrictness_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum LineBreakStrictness {
      LineBreakStrictness_Loose = 0,
      LineBreakStrictness_Normal = 1,
      LineBreakStrictness_Strict = 2,
      LineBreakStrictness_Anywhere = 3,
    };
    
    typedef struct LineBreakStrictness_option {union { LineBreakStrictness ok; }; bool is_ok; } LineBreakStrictness_option;
} // namespace capi
} // namespace

namespace icu4x {
class LineBreakStrictness {
public:
  enum Value {
    Loose = 0,
    Normal = 1,
    Strict = 2,
    Anywhere = 3,
  };

  LineBreakStrictness() = default;
  // Implicit conversions between enum and ::Value
  constexpr LineBreakStrictness(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::LineBreakStrictness AsFFI() const;
  inline static icu4x::LineBreakStrictness FromFFI(icu4x::capi::LineBreakStrictness c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LineBreakStrictness_D_HPP
