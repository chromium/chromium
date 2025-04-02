#ifndef icu4x_DateTimeMismatchedCalendarError_D_HPP
#define icu4x_DateTimeMismatchedCalendarError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "AnyCalendarKind.d.hpp"

namespace icu4x {
class AnyCalendarKind;
}


namespace icu4x {
namespace capi {
    struct DateTimeMismatchedCalendarError {
      icu4x::capi::AnyCalendarKind this_kind;
      icu4x::capi::AnyCalendarKind_option date_kind;
    };
    
    typedef struct DateTimeMismatchedCalendarError_option {union { DateTimeMismatchedCalendarError ok; }; bool is_ok; } DateTimeMismatchedCalendarError_option;
} // namespace capi
} // namespace


namespace icu4x {
struct DateTimeMismatchedCalendarError {
  icu4x::AnyCalendarKind this_kind;
  std::optional<icu4x::AnyCalendarKind> date_kind;

  inline icu4x::capi::DateTimeMismatchedCalendarError AsFFI() const;
  inline static icu4x::DateTimeMismatchedCalendarError FromFFI(icu4x::capi::DateTimeMismatchedCalendarError c_struct);
};

} // namespace
#endif // icu4x_DateTimeMismatchedCalendarError_D_HPP
