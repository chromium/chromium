#ifndef ICU4X_TimeZoneAndCanonical_D_HPP
#define ICU4X_TimeZoneAndCanonical_D_HPP

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
namespace capi { struct TimeZone; }
class TimeZone;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeZoneAndCanonical {
      icu4x::capi::TimeZone* time_zone;
      icu4x::diplomat::capi::DiplomatStringView canonical;
    };

    typedef struct TimeZoneAndCanonical_option {union { TimeZoneAndCanonical ok; }; bool is_ok; } TimeZoneAndCanonical_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `TimeZoneAndCanonical`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.TimeZoneAndCanonical.html) for more information.
 */
struct TimeZoneAndCanonical {
    std::unique_ptr<icu4x::TimeZone> time_zone;
    std::string_view canonical;

    inline icu4x::capi::TimeZoneAndCanonical AsFFI() const;
    inline static icu4x::TimeZoneAndCanonical FromFFI(icu4x::capi::TimeZoneAndCanonical c_struct);
};

} // namespace
#endif // ICU4X_TimeZoneAndCanonical_D_HPP
