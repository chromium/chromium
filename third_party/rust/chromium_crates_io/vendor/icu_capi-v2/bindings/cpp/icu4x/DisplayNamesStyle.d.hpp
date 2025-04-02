#ifndef icu4x_DisplayNamesStyle_D_HPP
#define icu4x_DisplayNamesStyle_D_HPP

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
    enum DisplayNamesStyle {
      DisplayNamesStyle_Narrow = 0,
      DisplayNamesStyle_Short = 1,
      DisplayNamesStyle_Long = 2,
      DisplayNamesStyle_Menu = 3,
    };
    
    typedef struct DisplayNamesStyle_option {union { DisplayNamesStyle ok; }; bool is_ok; } DisplayNamesStyle_option;
} // namespace capi
} // namespace

namespace icu4x {
class DisplayNamesStyle {
public:
  enum Value {
    Narrow = 0,
    Short = 1,
    Long = 2,
    Menu = 3,
  };

  DisplayNamesStyle() = default;
  // Implicit conversions between enum and ::Value
  constexpr DisplayNamesStyle(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::DisplayNamesStyle AsFFI() const;
  inline static icu4x::DisplayNamesStyle FromFFI(icu4x::capi::DisplayNamesStyle c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_DisplayNamesStyle_D_HPP
