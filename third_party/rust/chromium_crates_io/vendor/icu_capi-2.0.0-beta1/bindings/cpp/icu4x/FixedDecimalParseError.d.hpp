#ifndef icu4x_FixedDecimalParseError_D_HPP
#define icu4x_FixedDecimalParseError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum FixedDecimalParseError {
      FixedDecimalParseError_Unknown = 0,
      FixedDecimalParseError_Limit = 1,
      FixedDecimalParseError_Syntax = 2,
    };
    
    typedef struct FixedDecimalParseError_option {union { FixedDecimalParseError ok; }; bool is_ok; } FixedDecimalParseError_option;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimalParseError {
public:
  enum Value {
    Unknown = 0,
    Limit = 1,
    Syntax = 2,
  };

  FixedDecimalParseError() = default;
  // Implicit conversions between enum and ::Value
  constexpr FixedDecimalParseError(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::FixedDecimalParseError AsFFI() const;
  inline static icu4x::FixedDecimalParseError FromFFI(icu4x::capi::FixedDecimalParseError c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_FixedDecimalParseError_D_HPP
