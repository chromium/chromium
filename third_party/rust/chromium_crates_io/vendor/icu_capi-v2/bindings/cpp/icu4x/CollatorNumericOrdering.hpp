#ifndef ICU4X_CollatorNumericOrdering_HPP
#define ICU4X_CollatorNumericOrdering_HPP

#include "CollatorNumericOrdering.d.hpp"

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

inline icu4x::capi::CollatorNumericOrdering icu4x::CollatorNumericOrdering::AsFFI() const {
    return static_cast<icu4x::capi::CollatorNumericOrdering>(value);
}

inline icu4x::CollatorNumericOrdering icu4x::CollatorNumericOrdering::FromFFI(icu4x::capi::CollatorNumericOrdering c_enum) {
    switch (c_enum) {
        case icu4x::capi::CollatorNumericOrdering_Off:
        case icu4x::capi::CollatorNumericOrdering_On:
            return static_cast<icu4x::CollatorNumericOrdering::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CollatorNumericOrdering_HPP
