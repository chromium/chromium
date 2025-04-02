#ifndef icu4x_CollatorResolvedOptionsV1_D_HPP
#define icu4x_CollatorResolvedOptionsV1_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "CollatorAlternateHandling.d.hpp"
#include "CollatorBackwardSecondLevel.d.hpp"
#include "CollatorCaseFirst.d.hpp"
#include "CollatorCaseLevel.d.hpp"
#include "CollatorMaxVariable.d.hpp"
#include "CollatorNumericOrdering.d.hpp"
#include "CollatorStrength.d.hpp"

namespace icu4x {
class CollatorAlternateHandling;
class CollatorBackwardSecondLevel;
class CollatorCaseFirst;
class CollatorCaseLevel;
class CollatorMaxVariable;
class CollatorNumericOrdering;
class CollatorStrength;
}


namespace icu4x {
namespace capi {
    struct CollatorResolvedOptionsV1 {
      icu4x::capi::CollatorStrength strength;
      icu4x::capi::CollatorAlternateHandling alternate_handling;
      icu4x::capi::CollatorCaseFirst case_first;
      icu4x::capi::CollatorMaxVariable max_variable;
      icu4x::capi::CollatorCaseLevel case_level;
      icu4x::capi::CollatorNumericOrdering numeric;
      icu4x::capi::CollatorBackwardSecondLevel backward_second_level;
    };
    
    typedef struct CollatorResolvedOptionsV1_option {union { CollatorResolvedOptionsV1 ok; }; bool is_ok; } CollatorResolvedOptionsV1_option;
} // namespace capi
} // namespace


namespace icu4x {
struct CollatorResolvedOptionsV1 {
  icu4x::CollatorStrength strength;
  icu4x::CollatorAlternateHandling alternate_handling;
  icu4x::CollatorCaseFirst case_first;
  icu4x::CollatorMaxVariable max_variable;
  icu4x::CollatorCaseLevel case_level;
  icu4x::CollatorNumericOrdering numeric;
  icu4x::CollatorBackwardSecondLevel backward_second_level;

  inline icu4x::capi::CollatorResolvedOptionsV1 AsFFI() const;
  inline static icu4x::CollatorResolvedOptionsV1 FromFFI(icu4x::capi::CollatorResolvedOptionsV1 c_struct);
};

} // namespace
#endif // icu4x_CollatorResolvedOptionsV1_D_HPP
