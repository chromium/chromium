#ifndef icu4x_DateTimeFormatterLoadError_D_HPP
#define icu4x_DateTimeFormatterLoadError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum DateTimeFormatterLoadError {
      DateTimeFormatterLoadError_Unknown = 0,
      DateTimeFormatterLoadError_UnsupportedLength = 2051,
      DateTimeFormatterLoadError_DuplicateField = 2057,
      DateTimeFormatterLoadError_TypeTooSpecific = 2058,
      DateTimeFormatterLoadError_DataMarkerNotFound = 1,
      DateTimeFormatterLoadError_DataIdentifierNotFound = 2,
      DateTimeFormatterLoadError_DataInvalidRequest = 3,
      DateTimeFormatterLoadError_DataInconsistentData = 4,
      DateTimeFormatterLoadError_DataDowncast = 5,
      DateTimeFormatterLoadError_DataDeserialize = 6,
      DateTimeFormatterLoadError_DataCustom = 7,
      DateTimeFormatterLoadError_DataIo = 8,
    };
    
    typedef struct DateTimeFormatterLoadError_option {union { DateTimeFormatterLoadError ok; }; bool is_ok; } DateTimeFormatterLoadError_option;
} // namespace capi
} // namespace

namespace icu4x {
class DateTimeFormatterLoadError {
public:
  enum Value {
    Unknown = 0,
    UnsupportedLength = 2051,
    DuplicateField = 2057,
    TypeTooSpecific = 2058,
    DataMarkerNotFound = 1,
    DataIdentifierNotFound = 2,
    DataInvalidRequest = 3,
    DataInconsistentData = 4,
    DataDowncast = 5,
    DataDeserialize = 6,
    DataCustom = 7,
    DataIo = 8,
  };

  DateTimeFormatterLoadError() = default;
  // Implicit conversions between enum and ::Value
  constexpr DateTimeFormatterLoadError(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::DateTimeFormatterLoadError AsFFI() const;
  inline static icu4x::DateTimeFormatterLoadError FromFFI(icu4x::capi::DateTimeFormatterLoadError c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_DateTimeFormatterLoadError_D_HPP
