#ifndef icu4x_CollatorOptionsV1_HPP
#define icu4x_CollatorOptionsV1_HPP

#include "CollatorOptionsV1.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "CollatorAlternateHandling.hpp"
#include "CollatorBackwardSecondLevel.hpp"
#include "CollatorCaseFirst.hpp"
#include "CollatorCaseLevel.hpp"
#include "CollatorMaxVariable.hpp"
#include "CollatorNumeric.hpp"
#include "CollatorStrength.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
} // namespace capi
} // namespace


inline icu4x::capi::CollatorOptionsV1 icu4x::CollatorOptionsV1::AsFFI() const {
  return icu4x::capi::CollatorOptionsV1 {
    /* .strength = */ strength.has_value() ? (icu4x::capi::CollatorStrength_option{ { strength.value().AsFFI() }, true }) : (icu4x::capi::CollatorStrength_option{ {}, false }),
    /* .alternate_handling = */ alternate_handling.has_value() ? (icu4x::capi::CollatorAlternateHandling_option{ { alternate_handling.value().AsFFI() }, true }) : (icu4x::capi::CollatorAlternateHandling_option{ {}, false }),
    /* .case_first = */ case_first.has_value() ? (icu4x::capi::CollatorCaseFirst_option{ { case_first.value().AsFFI() }, true }) : (icu4x::capi::CollatorCaseFirst_option{ {}, false }),
    /* .max_variable = */ max_variable.has_value() ? (icu4x::capi::CollatorMaxVariable_option{ { max_variable.value().AsFFI() }, true }) : (icu4x::capi::CollatorMaxVariable_option{ {}, false }),
    /* .case_level = */ case_level.has_value() ? (icu4x::capi::CollatorCaseLevel_option{ { case_level.value().AsFFI() }, true }) : (icu4x::capi::CollatorCaseLevel_option{ {}, false }),
    /* .numeric = */ numeric.has_value() ? (icu4x::capi::CollatorNumeric_option{ { numeric.value().AsFFI() }, true }) : (icu4x::capi::CollatorNumeric_option{ {}, false }),
    /* .backward_second_level = */ backward_second_level.has_value() ? (icu4x::capi::CollatorBackwardSecondLevel_option{ { backward_second_level.value().AsFFI() }, true }) : (icu4x::capi::CollatorBackwardSecondLevel_option{ {}, false }),
  };
}

inline icu4x::CollatorOptionsV1 icu4x::CollatorOptionsV1::FromFFI(icu4x::capi::CollatorOptionsV1 c_struct) {
  return icu4x::CollatorOptionsV1 {
    /* .strength = */ c_struct.strength.is_ok ? std::optional(icu4x::CollatorStrength::FromFFI(c_struct.strength.ok)) : std::nullopt,
    /* .alternate_handling = */ c_struct.alternate_handling.is_ok ? std::optional(icu4x::CollatorAlternateHandling::FromFFI(c_struct.alternate_handling.ok)) : std::nullopt,
    /* .case_first = */ c_struct.case_first.is_ok ? std::optional(icu4x::CollatorCaseFirst::FromFFI(c_struct.case_first.ok)) : std::nullopt,
    /* .max_variable = */ c_struct.max_variable.is_ok ? std::optional(icu4x::CollatorMaxVariable::FromFFI(c_struct.max_variable.ok)) : std::nullopt,
    /* .case_level = */ c_struct.case_level.is_ok ? std::optional(icu4x::CollatorCaseLevel::FromFFI(c_struct.case_level.ok)) : std::nullopt,
    /* .numeric = */ c_struct.numeric.is_ok ? std::optional(icu4x::CollatorNumeric::FromFFI(c_struct.numeric.ok)) : std::nullopt,
    /* .backward_second_level = */ c_struct.backward_second_level.is_ok ? std::optional(icu4x::CollatorBackwardSecondLevel::FromFFI(c_struct.backward_second_level.ok)) : std::nullopt,
  };
}


#endif // icu4x_CollatorOptionsV1_HPP
