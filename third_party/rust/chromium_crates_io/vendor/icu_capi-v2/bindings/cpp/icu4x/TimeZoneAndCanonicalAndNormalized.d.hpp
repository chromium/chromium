#ifndef ICU4X_TimeZoneAndCanonicalAndNormalized_D_HPP
#define ICU4X_TimeZoneAndCanonicalAndNormalized_D_HPP

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
    struct TimeZoneAndCanonicalAndNormalized {
      icu4x::capi::TimeZone* time_zone;
      icu4x::diplomat::capi::DiplomatStringView canonical;
      icu4x::diplomat::capi::DiplomatStringView normalized;
    };

    typedef struct TimeZoneAndCanonicalAndNormalized_option {union { TimeZoneAndCanonicalAndNormalized ok; }; bool is_ok; } TimeZoneAndCanonicalAndNormalized_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `TimeZoneAndCanonicalAndNormalized`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.TimeZoneAndCanonicalAndNormalized.html) for more information.
 */
struct TimeZoneAndCanonicalAndNormalized {
    std::unique_ptr<icu4x::TimeZone> time_zone;
    std::string_view canonical;
    std::string_view normalized;

    inline icu4x::capi::TimeZoneAndCanonicalAndNormalized AsFFI() const;
    inline static icu4x::TimeZoneAndCanonicalAndNormalized FromFFI(icu4x::capi::TimeZoneAndCanonicalAndNormalized c_struct);
};

} // namespace
#endif // ICU4X_TimeZoneAndCanonicalAndNormalized_D_HPP
