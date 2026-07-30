#ifndef ICU4X_TimeZoneAndCanonicalAndNormalized_HPP
#define ICU4X_TimeZoneAndCanonicalAndNormalized_HPP

#include "TimeZoneAndCanonicalAndNormalized.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "TimeZone.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::TimeZoneAndCanonicalAndNormalized icu4x::TimeZoneAndCanonicalAndNormalized::AsFFI() const {
    return icu4x::capi::TimeZoneAndCanonicalAndNormalized {
        /* .time_zone = */ time_zone->AsFFI(),
        /* .canonical = */ {canonical.data(), canonical.size()},
        /* .normalized = */ {normalized.data(), normalized.size()},
    };
}

inline icu4x::TimeZoneAndCanonicalAndNormalized icu4x::TimeZoneAndCanonicalAndNormalized::FromFFI(icu4x::capi::TimeZoneAndCanonicalAndNormalized c_struct) {
    return icu4x::TimeZoneAndCanonicalAndNormalized {
        /* .time_zone = */ std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(c_struct.time_zone)),
        /* .canonical = */ std::string_view(c_struct.canonical.data, c_struct.canonical.len),
        /* .normalized = */ std::string_view(c_struct.normalized.data, c_struct.normalized.len),
    };
}


#endif // ICU4X_TimeZoneAndCanonicalAndNormalized_HPP
