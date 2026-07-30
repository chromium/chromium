#ifndef ICU4X_CollatorCaseLevel_HPP
#define ICU4X_CollatorCaseLevel_HPP

#include "CollatorCaseLevel.d.hpp"

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

inline icu4x::capi::CollatorCaseLevel icu4x::CollatorCaseLevel::AsFFI() const {
    return static_cast<icu4x::capi::CollatorCaseLevel>(value);
}

inline icu4x::CollatorCaseLevel icu4x::CollatorCaseLevel::FromFFI(icu4x::capi::CollatorCaseLevel c_enum) {
    switch (c_enum) {
        case icu4x::capi::CollatorCaseLevel_Off:
        case icu4x::capi::CollatorCaseLevel_On:
            return static_cast<icu4x::CollatorCaseLevel::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CollatorCaseLevel_HPP
