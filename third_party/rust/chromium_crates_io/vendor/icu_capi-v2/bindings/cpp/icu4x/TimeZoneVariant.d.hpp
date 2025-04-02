#ifndef icu4x_TimeZoneVariant_D_HPP
#define icu4x_TimeZoneVariant_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class TimeZoneVariant;
}


namespace icu4x {
namespace capi {
    enum TimeZoneVariant {
      TimeZoneVariant_Standard = 0,
      TimeZoneVariant_Daylight = 1,
    };
    
    typedef struct TimeZoneVariant_option {union { TimeZoneVariant ok; }; bool is_ok; } TimeZoneVariant_option;
} // namespace capi
} // namespace

namespace icu4x {
class TimeZoneVariant {
public:
  enum Value {
    Standard = 0,
    Daylight = 1,
  };

  TimeZoneVariant() = default;
  // Implicit conversions between enum and ::Value
  constexpr TimeZoneVariant(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::TimeZoneVariant from_rearguard_isdst(bool isdst);

  inline icu4x::capi::TimeZoneVariant AsFFI() const;
  inline static icu4x::TimeZoneVariant FromFFI(icu4x::capi::TimeZoneVariant c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_TimeZoneVariant_D_HPP
