#ifndef ICU4X_DateTimeMismatchedCalendarError_D_HPP
#define ICU4X_DateTimeMismatchedCalendarError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CalendarKind.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
class CalendarKind;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateTimeMismatchedCalendarError {
      icu4x::capi::CalendarKind this_kind;
      icu4x::capi::CalendarKind_option date_kind;
    };

    typedef struct DateTimeMismatchedCalendarError_option {union { DateTimeMismatchedCalendarError ok; }; bool is_ok; } DateTimeMismatchedCalendarError_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `MismatchedCalendarError`](https://docs.rs/icu/2.2.0/icu/datetime/struct.MismatchedCalendarError.html) for more information.
 */
struct DateTimeMismatchedCalendarError {
    icu4x::CalendarKind this_kind;
    std::optional<icu4x::CalendarKind> date_kind;

    inline icu4x::capi::DateTimeMismatchedCalendarError AsFFI() const;
    inline static icu4x::DateTimeMismatchedCalendarError FromFFI(icu4x::capi::DateTimeMismatchedCalendarError c_struct);
};

} // namespace
#endif // ICU4X_DateTimeMismatchedCalendarError_D_HPP
