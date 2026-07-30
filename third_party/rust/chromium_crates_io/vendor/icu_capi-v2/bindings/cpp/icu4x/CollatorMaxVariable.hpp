#ifndef ICU4X_CollatorMaxVariable_HPP
#define ICU4X_CollatorMaxVariable_HPP

#include "CollatorMaxVariable.d.hpp"

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

inline icu4x::capi::CollatorMaxVariable icu4x::CollatorMaxVariable::AsFFI() const {
    return static_cast<icu4x::capi::CollatorMaxVariable>(value);
}

inline icu4x::CollatorMaxVariable icu4x::CollatorMaxVariable::FromFFI(icu4x::capi::CollatorMaxVariable c_enum) {
    switch (c_enum) {
        case icu4x::capi::CollatorMaxVariable_Space:
        case icu4x::capi::CollatorMaxVariable_Punctuation:
        case icu4x::capi::CollatorMaxVariable_Symbol:
        case icu4x::capi::CollatorMaxVariable_Currency:
            return static_cast<icu4x::CollatorMaxVariable::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CollatorMaxVariable_HPP
