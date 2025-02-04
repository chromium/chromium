#ifndef icu4x_LocaleDirection_D_HPP
#define icu4x_LocaleDirection_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum LocaleDirection {
      LocaleDirection_LeftToRight = 0,
      LocaleDirection_RightToLeft = 1,
      LocaleDirection_Unknown = 2,
    };
    
    typedef struct LocaleDirection_option {union { LocaleDirection ok; }; bool is_ok; } LocaleDirection_option;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleDirection {
public:
  enum Value {
    LeftToRight = 0,
    RightToLeft = 1,
    Unknown = 2,
  };

  LocaleDirection() = default;
  // Implicit conversions between enum and ::Value
  constexpr LocaleDirection(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::LocaleDirection AsFFI() const;
  inline static icu4x::LocaleDirection FromFFI(icu4x::capi::LocaleDirection c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LocaleDirection_D_HPP
