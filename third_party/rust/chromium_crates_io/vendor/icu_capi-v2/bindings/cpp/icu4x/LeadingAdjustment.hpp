#ifndef ICU4X_LeadingAdjustment_HPP
#define ICU4X_LeadingAdjustment_HPP

#include "LeadingAdjustment.d.hpp"

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

} // namespace capi
} // namespace

inline icu4x::capi::LeadingAdjustment icu4x::LeadingAdjustment::AsFFI() const {
    return static_cast<icu4x::capi::LeadingAdjustment>(value);
}

inline icu4x::LeadingAdjustment icu4x::LeadingAdjustment::FromFFI(icu4x::capi::LeadingAdjustment c_enum) {
    switch (c_enum) {
        case icu4x::capi::LeadingAdjustment_Auto:
        case icu4x::capi::LeadingAdjustment_None:
        case icu4x::capi::LeadingAdjustment_ToCased:
            return static_cast<icu4x::LeadingAdjustment::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_LeadingAdjustment_HPP
