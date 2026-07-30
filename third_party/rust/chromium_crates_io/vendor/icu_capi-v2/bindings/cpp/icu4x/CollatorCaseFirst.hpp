#ifndef ICU4X_CollatorCaseFirst_HPP
#define ICU4X_CollatorCaseFirst_HPP

#include "CollatorCaseFirst.d.hpp"

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

inline icu4x::capi::CollatorCaseFirst icu4x::CollatorCaseFirst::AsFFI() const {
    return static_cast<icu4x::capi::CollatorCaseFirst>(value);
}

inline icu4x::CollatorCaseFirst icu4x::CollatorCaseFirst::FromFFI(icu4x::capi::CollatorCaseFirst c_enum) {
    switch (c_enum) {
        case icu4x::capi::CollatorCaseFirst_Off:
        case icu4x::capi::CollatorCaseFirst_Lower:
        case icu4x::capi::CollatorCaseFirst_Upper:
            return static_cast<icu4x::CollatorCaseFirst::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CollatorCaseFirst_HPP
