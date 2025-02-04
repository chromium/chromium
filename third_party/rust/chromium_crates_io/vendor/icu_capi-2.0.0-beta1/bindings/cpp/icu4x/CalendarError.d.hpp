#ifndef icu4x_CalendarError_D_HPP
#define icu4x_CalendarError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum CalendarError {
      CalendarError_Unknown = 0,
      CalendarError_OutOfRange = 1,
      CalendarError_UnknownEra = 2,
      CalendarError_UnknownMonthCode = 3,
    };
    
    typedef struct CalendarError_option {union { CalendarError ok; }; bool is_ok; } CalendarError_option;
} // namespace capi
} // namespace

namespace icu4x {
class CalendarError {
public:
  enum Value {
    Unknown = 0,
    OutOfRange = 1,
    UnknownEra = 2,
    UnknownMonthCode = 3,
  };

  CalendarError() = default;
  // Implicit conversions between enum and ::Value
  constexpr CalendarError(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::CalendarError AsFFI() const;
  inline static icu4x::CalendarError FromFFI(icu4x::capi::CalendarError c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_CalendarError_D_HPP
