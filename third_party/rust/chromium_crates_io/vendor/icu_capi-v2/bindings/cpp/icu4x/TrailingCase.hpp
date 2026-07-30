#ifndef ICU4X_TrailingCase_HPP
#define ICU4X_TrailingCase_HPP

#include "TrailingCase.d.hpp"

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

inline icu4x::capi::TrailingCase icu4x::TrailingCase::AsFFI() const {
    return static_cast<icu4x::capi::TrailingCase>(value);
}

inline icu4x::TrailingCase icu4x::TrailingCase::FromFFI(icu4x::capi::TrailingCase c_enum) {
    switch (c_enum) {
        case icu4x::capi::TrailingCase_Lower:
        case icu4x::capi::TrailingCase_Unchanged:
            return static_cast<icu4x::TrailingCase::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_TrailingCase_HPP
