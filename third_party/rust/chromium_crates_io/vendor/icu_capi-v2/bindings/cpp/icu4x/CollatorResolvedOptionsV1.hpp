#ifndef ICU4X_CollatorResolvedOptionsV1_HPP
#define ICU4X_CollatorResolvedOptionsV1_HPP

#include "CollatorResolvedOptionsV1.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CollatorAlternateHandling.hpp"
#include "CollatorCaseFirst.hpp"
#include "CollatorCaseLevel.hpp"
#include "CollatorMaxVariable.hpp"
#include "CollatorNumericOrdering.hpp"
#include "CollatorStrength.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::CollatorResolvedOptionsV1 icu4x::CollatorResolvedOptionsV1::AsFFI() const {
    return icu4x::capi::CollatorResolvedOptionsV1 {
        /* .strength = */ strength.AsFFI(),
        /* .alternate_handling = */ alternate_handling.AsFFI(),
        /* .case_first = */ case_first.AsFFI(),
        /* .max_variable = */ max_variable.AsFFI(),
        /* .case_level = */ case_level.AsFFI(),
        /* .numeric = */ numeric.AsFFI(),
    };
}

inline icu4x::CollatorResolvedOptionsV1 icu4x::CollatorResolvedOptionsV1::FromFFI(icu4x::capi::CollatorResolvedOptionsV1 c_struct) {
    return icu4x::CollatorResolvedOptionsV1 {
        /* .strength = */ icu4x::CollatorStrength::FromFFI(c_struct.strength),
        /* .alternate_handling = */ icu4x::CollatorAlternateHandling::FromFFI(c_struct.alternate_handling),
        /* .case_first = */ icu4x::CollatorCaseFirst::FromFFI(c_struct.case_first),
        /* .max_variable = */ icu4x::CollatorMaxVariable::FromFFI(c_struct.max_variable),
        /* .case_level = */ icu4x::CollatorCaseLevel::FromFFI(c_struct.case_level),
        /* .numeric = */ icu4x::CollatorNumericOrdering::FromFFI(c_struct.numeric),
    };
}


#endif // ICU4X_CollatorResolvedOptionsV1_HPP
