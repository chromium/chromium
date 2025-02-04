#ifndef icu4x_DateTimeFormatError_D_HPP
#define icu4x_DateTimeFormatError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum DateTimeFormatError {
      DateTimeFormatError_Unknown = 0,
      DateTimeFormatError_MissingInputField = 1,
      DateTimeFormatError_ZoneInfoMissingFields = 2,
      DateTimeFormatError_InvalidEra = 3,
      DateTimeFormatError_InvalidMonthCode = 4,
      DateTimeFormatError_InvalidCyclicYear = 5,
      DateTimeFormatError_NamesNotLoaded = 16,
      DateTimeFormatError_FixedDecimalFormatterNotLoaded = 17,
      DateTimeFormatError_UnsupportedField = 18,
    };
    
    typedef struct DateTimeFormatError_option {union { DateTimeFormatError ok; }; bool is_ok; } DateTimeFormatError_option;
} // namespace capi
} // namespace

namespace icu4x {
class DateTimeFormatError {
public:
  enum Value {
    Unknown = 0,
    MissingInputField = 1,
    ZoneInfoMissingFields = 2,
    InvalidEra = 3,
    InvalidMonthCode = 4,
    InvalidCyclicYear = 5,
    NamesNotLoaded = 16,
    FixedDecimalFormatterNotLoaded = 17,
    UnsupportedField = 18,
  };

  DateTimeFormatError() = default;
  // Implicit conversions between enum and ::Value
  constexpr DateTimeFormatError(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::DateTimeFormatError AsFFI() const;
  inline static icu4x::DateTimeFormatError FromFFI(icu4x::capi::DateTimeFormatError c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_DateTimeFormatError_D_HPP
