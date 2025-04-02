#ifndef icu4x_IsoDateTime_D_HPP
#define icu4x_IsoDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
struct IsoDateTime;
class CalendarParseError;
}


namespace icu4x {
namespace capi {
    struct IsoDateTime {
      icu4x::capi::IsoDate* date;
      icu4x::capi::Time* time;
    };
    
    typedef struct IsoDateTime_option {union { IsoDateTime ok; }; bool is_ok; } IsoDateTime_option;
} // namespace capi
} // namespace


namespace icu4x {
struct IsoDateTime {
  std::unique_ptr<icu4x::IsoDate> date;
  std::unique_ptr<icu4x::Time> time;

  inline static diplomat::result<icu4x::IsoDateTime, icu4x::CalendarParseError> from_string(std::string_view v);

  inline icu4x::capi::IsoDateTime AsFFI() const;
  inline static icu4x::IsoDateTime FromFFI(icu4x::capi::IsoDateTime c_struct);
};

} // namespace
#endif // icu4x_IsoDateTime_D_HPP
