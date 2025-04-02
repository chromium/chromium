#ifndef icu4x_TimeZoneAndCanonicalAndNormalized_D_HPP
#define icu4x_TimeZoneAndCanonicalAndNormalized_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct TimeZone; }
class TimeZone;
}


namespace icu4x {
namespace capi {
    struct TimeZoneAndCanonicalAndNormalized {
      icu4x::capi::TimeZone* time_zone;
      diplomat::capi::DiplomatStringView canonical;
      diplomat::capi::DiplomatStringView normalized;
    };
    
    typedef struct TimeZoneAndCanonicalAndNormalized_option {union { TimeZoneAndCanonicalAndNormalized ok; }; bool is_ok; } TimeZoneAndCanonicalAndNormalized_option;
} // namespace capi
} // namespace


namespace icu4x {
struct TimeZoneAndCanonicalAndNormalized {
  std::unique_ptr<icu4x::TimeZone> time_zone;
  std::string_view canonical;
  std::string_view normalized;

  inline icu4x::capi::TimeZoneAndCanonicalAndNormalized AsFFI() const;
  inline static icu4x::TimeZoneAndCanonicalAndNormalized FromFFI(icu4x::capi::TimeZoneAndCanonicalAndNormalized c_struct);
};

} // namespace
#endif // icu4x_TimeZoneAndCanonicalAndNormalized_D_HPP
