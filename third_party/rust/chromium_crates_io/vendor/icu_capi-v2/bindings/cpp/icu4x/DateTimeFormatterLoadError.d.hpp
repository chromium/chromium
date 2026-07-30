#ifndef ICU4X_DateTimeFormatterLoadError_D_HPP
#define ICU4X_DateTimeFormatterLoadError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum DateTimeFormatterLoadError {
      DateTimeFormatterLoadError_Unknown = 0,
      DateTimeFormatterLoadError_InvalidDateFields = 2049,
      DateTimeFormatterLoadError_UnsupportedLength = 2051,
      DateTimeFormatterLoadError_ConflictingField = 2057,
      DateTimeFormatterLoadError_FormatterTooSpecific = 2058,
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
/**
 * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/enum.DateTimeFormatterLoadError.html), [2](https://docs.rs/icu/2.2.0/icu/datetime/pattern/enum.PatternLoadError.html), [3](https://docs.rs/icu_provider/2.2.0/icu_provider/struct.DataError.html), [4](https://docs.rs/icu_provider/2.2.0/icu_provider/enum.DataErrorKind.html)
 */
class DateTimeFormatterLoadError {
public:
    enum Value {
        Unknown = 0,
        InvalidDateFields = 2049,
        UnsupportedLength = 2051,
        ConflictingField = 2057,
        FormatterTooSpecific = 2058,
        DataMarkerNotFound = 1,
        DataIdentifierNotFound = 2,
        DataInvalidRequest = 3,
        DataInconsistentData = 4,
        DataDowncast = 5,
        DataDeserialize = 6,
        DataCustom = 7,
        DataIo = 8,
    };

    DateTimeFormatterLoadError(): value(Value::Unknown) {}

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
#endif // ICU4X_DateTimeFormatterLoadError_D_HPP
