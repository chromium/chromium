#ifndef icu4x_ZonedIsoDateTime_D_HPP
#define icu4x_ZonedIsoDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct IanaParser; }
class IanaParser;
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffsetCalculator; }
class UtcOffsetCalculator;
struct ZonedIsoDateTime;
class CalendarParseError;
}


namespace icu4x {
namespace capi {
    struct ZonedIsoDateTime {
      icu4x::capi::IsoDate* date;
      icu4x::capi::Time* time;
      icu4x::capi::TimeZoneInfo* zone;
    };
    
    typedef struct ZonedIsoDateTime_option {union { ZonedIsoDateTime ok; }; bool is_ok; } ZonedIsoDateTime_option;
} // namespace capi
} // namespace


namespace icu4x {
struct ZonedIsoDateTime {
  std::unique_ptr<icu4x::IsoDate> date;
  std::unique_ptr<icu4x::Time> time;
  std::unique_ptr<icu4x::TimeZoneInfo> zone;

  inline static diplomat::result<icu4x::ZonedIsoDateTime, icu4x::CalendarParseError> from_string(std::string_view v, const icu4x::IanaParser& iana_parser, const icu4x::UtcOffsetCalculator& offset_calculator);

  inline icu4x::capi::ZonedIsoDateTime AsFFI() const;
  inline static icu4x::ZonedIsoDateTime FromFFI(icu4x::capi::ZonedIsoDateTime c_struct);
};

} // namespace
#endif // icu4x_ZonedIsoDateTime_D_HPP
