#ifndef icu4x_CalendarParseError_D_HPP
#define icu4x_CalendarParseError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum CalendarParseError {
      CalendarParseError_Unknown = 0,
      CalendarParseError_InvalidSyntax = 1,
      CalendarParseError_OutOfRange = 2,
      CalendarParseError_MissingFields = 3,
      CalendarParseError_UnknownCalendar = 4,
    };
    
    typedef struct CalendarParseError_option {union { CalendarParseError ok; }; bool is_ok; } CalendarParseError_option;
} // namespace capi
} // namespace

namespace icu4x {
class CalendarParseError {
public:
  enum Value {
    Unknown = 0,
    InvalidSyntax = 1,
    OutOfRange = 2,
    MissingFields = 3,
    UnknownCalendar = 4,
  };

  CalendarParseError() = default;
  // Implicit conversions between enum and ::Value
  constexpr CalendarParseError(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::CalendarParseError AsFFI() const;
  inline static icu4x::CalendarParseError FromFFI(icu4x::capi::CalendarParseError c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_CalendarParseError_D_HPP
