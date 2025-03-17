#ifndef icu4x_DateTime_D_HPP
#define icu4x_DateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Date; }
class Date;
namespace capi { struct Time; }
class Time;
struct DateTime;
class CalendarParseError;
}


namespace icu4x {
namespace capi {
    struct DateTime {
      icu4x::capi::Date* date;
      icu4x::capi::Time* time;
    };
    
    typedef struct DateTime_option {union { DateTime ok; }; bool is_ok; } DateTime_option;
} // namespace capi
} // namespace


namespace icu4x {
struct DateTime {
  std::unique_ptr<icu4x::Date> date;
  std::unique_ptr<icu4x::Time> time;

  inline static diplomat::result<icu4x::DateTime, icu4x::CalendarParseError> from_string(std::string_view v, const icu4x::Calendar& calendar);

  inline icu4x::capi::DateTime AsFFI() const;
  inline static icu4x::DateTime FromFFI(icu4x::capi::DateTime c_struct);
};

} // namespace
#endif // icu4x_DateTime_D_HPP
