#ifndef ICU4X_Rfc9557ParseError_D_HPP
#define ICU4X_Rfc9557ParseError_D_HPP

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
    enum Rfc9557ParseError {
      Rfc9557ParseError_Unknown = 0,
      Rfc9557ParseError_InvalidSyntax = 1,
      Rfc9557ParseError_OutOfRange = 2,
      Rfc9557ParseError_MissingFields = 3,
      Rfc9557ParseError_UnknownCalendar = 4,
    };

    typedef struct Rfc9557ParseError_option {union { Rfc9557ParseError ok; }; bool is_ok; } Rfc9557ParseError_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/enum.ParseError.html), [2](https://docs.rs/icu/2.2.0/icu/time/enum.ParseError.html)
 */
class Rfc9557ParseError {
public:
    enum Value {
        Unknown = 0,
        InvalidSyntax = 1,
        OutOfRange = 2,
        MissingFields = 3,
        UnknownCalendar = 4,
    };

    Rfc9557ParseError(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr Rfc9557ParseError(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::Rfc9557ParseError AsFFI() const;
    inline static icu4x::Rfc9557ParseError FromFFI(icu4x::capi::Rfc9557ParseError c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_Rfc9557ParseError_D_HPP
