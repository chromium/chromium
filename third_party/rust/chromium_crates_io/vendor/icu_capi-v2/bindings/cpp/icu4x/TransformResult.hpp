#ifndef ICU4X_TransformResult_HPP
#define ICU4X_TransformResult_HPP

#include "TransformResult.d.hpp"

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

inline icu4x::capi::TransformResult icu4x::TransformResult::AsFFI() const {
    return static_cast<icu4x::capi::TransformResult>(value);
}

inline icu4x::TransformResult icu4x::TransformResult::FromFFI(icu4x::capi::TransformResult c_enum) {
    switch (c_enum) {
        case icu4x::capi::TransformResult_Modified:
        case icu4x::capi::TransformResult_Unmodified:
            return static_cast<icu4x::TransformResult::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_TransformResult_HPP
