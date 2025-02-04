#ifndef icu4x_CollatorCaseLevel_D_HPP
#define icu4x_CollatorCaseLevel_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum CollatorCaseLevel {
      CollatorCaseLevel_Off = 0,
      CollatorCaseLevel_On = 1,
    };
    
    typedef struct CollatorCaseLevel_option {union { CollatorCaseLevel ok; }; bool is_ok; } CollatorCaseLevel_option;
} // namespace capi
} // namespace

namespace icu4x {
class CollatorCaseLevel {
public:
  enum Value {
    Off = 0,
    On = 1,
  };

  CollatorCaseLevel() = default;
  // Implicit conversions between enum and ::Value
  constexpr CollatorCaseLevel(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::CollatorCaseLevel AsFFI() const;
  inline static icu4x::CollatorCaseLevel FromFFI(icu4x::capi::CollatorCaseLevel c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_CollatorCaseLevel_D_HPP
