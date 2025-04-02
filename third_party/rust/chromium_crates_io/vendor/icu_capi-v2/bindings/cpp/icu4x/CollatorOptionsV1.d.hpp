#ifndef icu4x_CollatorOptionsV1_D_HPP
#define icu4x_CollatorOptionsV1_D_HPP

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
#include "CollatorCaseLevel.d.hpp"
#include "CollatorMaxVariable.d.hpp"
#include "CollatorStrength.d.hpp"

namespace icu4x {
class CollatorAlternateHandling;
class CollatorBackwardSecondLevel;
class CollatorCaseLevel;
class CollatorMaxVariable;
class CollatorStrength;
}


namespace icu4x {
namespace capi {
    struct CollatorOptionsV1 {
      icu4x::capi::CollatorStrength_option strength;
      icu4x::capi::CollatorAlternateHandling_option alternate_handling;
      icu4x::capi::CollatorMaxVariable_option max_variable;
      icu4x::capi::CollatorCaseLevel_option case_level;
      icu4x::capi::CollatorBackwardSecondLevel_option backward_second_level;
    };
    
    typedef struct CollatorOptionsV1_option {union { CollatorOptionsV1 ok; }; bool is_ok; } CollatorOptionsV1_option;
} // namespace capi
} // namespace


namespace icu4x {
struct CollatorOptionsV1 {
  std::optional<icu4x::CollatorStrength> strength;
  std::optional<icu4x::CollatorAlternateHandling> alternate_handling;
  std::optional<icu4x::CollatorMaxVariable> max_variable;
  std::optional<icu4x::CollatorCaseLevel> case_level;
  std::optional<icu4x::CollatorBackwardSecondLevel> backward_second_level;

  inline icu4x::capi::CollatorOptionsV1 AsFFI() const;
  inline static icu4x::CollatorOptionsV1 FromFFI(icu4x::capi::CollatorOptionsV1 c_struct);
};

} // namespace
#endif // icu4x_CollatorOptionsV1_D_HPP
