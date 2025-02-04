#ifndef icu4x_ListLength_D_HPP
#define icu4x_ListLength_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum ListLength {
      ListLength_Wide = 0,
      ListLength_Short = 1,
      ListLength_Narrow = 2,
    };
    
    typedef struct ListLength_option {union { ListLength ok; }; bool is_ok; } ListLength_option;
} // namespace capi
} // namespace

namespace icu4x {
class ListLength {
public:
  enum Value {
    Wide = 0,
    Short = 1,
    Narrow = 2,
  };

  ListLength() = default;
  // Implicit conversions between enum and ::Value
  constexpr ListLength(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::ListLength AsFFI() const;
  inline static icu4x::ListLength FromFFI(icu4x::capi::ListLength c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_ListLength_D_HPP
