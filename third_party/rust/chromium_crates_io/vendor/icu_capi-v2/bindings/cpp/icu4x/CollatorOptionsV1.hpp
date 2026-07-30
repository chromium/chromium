#ifndef ICU4X_CollatorOptionsV1_HPP
#define ICU4X_CollatorOptionsV1_HPP

#include "CollatorOptionsV1.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CollatorAlternateHandling.hpp"
#include "CollatorCaseLevel.hpp"
#include "CollatorMaxVariable.hpp"
#include "CollatorStrength.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::CollatorOptionsV1 icu4x::CollatorOptionsV1::AsFFI() const {
    return icu4x::capi::CollatorOptionsV1 {
        /* .strength = */ strength.has_value() ? (icu4x::capi::CollatorStrength_option{ { strength.value().AsFFI() }, true }) : (icu4x::capi::CollatorStrength_option{ {}, false }),
        /* .alternate_handling = */ alternate_handling.has_value() ? (icu4x::capi::CollatorAlternateHandling_option{ { alternate_handling.value().AsFFI() }, true }) : (icu4x::capi::CollatorAlternateHandling_option{ {}, false }),
        /* .max_variable = */ max_variable.has_value() ? (icu4x::capi::CollatorMaxVariable_option{ { max_variable.value().AsFFI() }, true }) : (icu4x::capi::CollatorMaxVariable_option{ {}, false }),
        /* .case_level = */ case_level.has_value() ? (icu4x::capi::CollatorCaseLevel_option{ { case_level.value().AsFFI() }, true }) : (icu4x::capi::CollatorCaseLevel_option{ {}, false }),
    };
}

inline icu4x::CollatorOptionsV1 icu4x::CollatorOptionsV1::FromFFI(icu4x::capi::CollatorOptionsV1 c_struct) {
    return icu4x::CollatorOptionsV1 {
        /* .strength = */ c_struct.strength.is_ok ? std::optional(icu4x::CollatorStrength::FromFFI(c_struct.strength.ok)) : std::nullopt,
        /* .alternate_handling = */ c_struct.alternate_handling.is_ok ? std::optional(icu4x::CollatorAlternateHandling::FromFFI(c_struct.alternate_handling.ok)) : std::nullopt,
        /* .max_variable = */ c_struct.max_variable.is_ok ? std::optional(icu4x::CollatorMaxVariable::FromFFI(c_struct.max_variable.ok)) : std::nullopt,
        /* .case_level = */ c_struct.case_level.is_ok ? std::optional(icu4x::CollatorCaseLevel::FromFFI(c_struct.case_level.ok)) : std::nullopt,
    };
}


#endif // ICU4X_CollatorOptionsV1_HPP
