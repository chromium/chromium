#ifndef icu4x_LineBreakWordOption_D_HPP
#define icu4x_LineBreakWordOption_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum LineBreakWordOption {
      LineBreakWordOption_Normal = 0,
      LineBreakWordOption_BreakAll = 1,
      LineBreakWordOption_KeepAll = 2,
    };
    
    typedef struct LineBreakWordOption_option {union { LineBreakWordOption ok; }; bool is_ok; } LineBreakWordOption_option;
} // namespace capi
} // namespace

namespace icu4x {
class LineBreakWordOption {
public:
  enum Value {
    Normal = 0,
    BreakAll = 1,
    KeepAll = 2,
  };

  LineBreakWordOption() = default;
  // Implicit conversions between enum and ::Value
  constexpr LineBreakWordOption(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::LineBreakWordOption AsFFI() const;
  inline static icu4x::LineBreakWordOption FromFFI(icu4x::capi::LineBreakWordOption c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LineBreakWordOption_D_HPP
