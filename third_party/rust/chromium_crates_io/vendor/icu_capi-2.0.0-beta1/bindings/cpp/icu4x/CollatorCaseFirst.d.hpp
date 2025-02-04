#ifndef icu4x_CollatorCaseFirst_D_HPP
#define icu4x_CollatorCaseFirst_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum CollatorCaseFirst {
      CollatorCaseFirst_Off = 0,
      CollatorCaseFirst_LowerFirst = 1,
      CollatorCaseFirst_UpperFirst = 2,
    };
    
    typedef struct CollatorCaseFirst_option {union { CollatorCaseFirst ok; }; bool is_ok; } CollatorCaseFirst_option;
} // namespace capi
} // namespace

namespace icu4x {
class CollatorCaseFirst {
public:
  enum Value {
    Off = 0,
    LowerFirst = 1,
    UpperFirst = 2,
  };

  CollatorCaseFirst() = default;
  // Implicit conversions between enum and ::Value
  constexpr CollatorCaseFirst(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::CollatorCaseFirst AsFFI() const;
  inline static icu4x::CollatorCaseFirst FromFFI(icu4x::capi::CollatorCaseFirst c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_CollatorCaseFirst_D_HPP
