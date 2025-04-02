#ifndef icu4x_LeadingAdjustment_D_HPP
#define icu4x_LeadingAdjustment_D_HPP

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
    enum LeadingAdjustment {
      LeadingAdjustment_Auto = 0,
      LeadingAdjustment_None = 1,
      LeadingAdjustment_ToCased = 2,
    };
    
    typedef struct LeadingAdjustment_option {union { LeadingAdjustment ok; }; bool is_ok; } LeadingAdjustment_option;
} // namespace capi
} // namespace

namespace icu4x {
class LeadingAdjustment {
public:
  enum Value {
    Auto = 0,
    None = 1,
    ToCased = 2,
  };

  LeadingAdjustment() = default;
  // Implicit conversions between enum and ::Value
  constexpr LeadingAdjustment(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::LeadingAdjustment AsFFI() const;
  inline static icu4x::LeadingAdjustment FromFFI(icu4x::capi::LeadingAdjustment c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LeadingAdjustment_D_HPP
