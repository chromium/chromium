#ifndef ICU4X_CollatorOptionsV1_D_HPP
#define ICU4X_CollatorOptionsV1_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CollatorAlternateHandling.d.hpp"
#include "CollatorCaseLevel.d.hpp"
#include "CollatorMaxVariable.d.hpp"
#include "CollatorStrength.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
class CollatorAlternateHandling;
class CollatorCaseLevel;
class CollatorMaxVariable;
class CollatorStrength;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CollatorOptionsV1 {
      icu4x::capi::CollatorStrength_option strength;
      icu4x::capi::CollatorAlternateHandling_option alternate_handling;
      icu4x::capi::CollatorMaxVariable_option max_variable;
      icu4x::capi::CollatorCaseLevel_option case_level;
    };

    typedef struct CollatorOptionsV1_option {union { CollatorOptionsV1 ok; }; bool is_ok; } CollatorOptionsV1_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `CollatorOptions`](https://docs.rs/icu/2.2.0/icu/collator/options/struct.CollatorOptions.html) for more information.
 */
struct CollatorOptionsV1 {
    std::optional<icu4x::CollatorStrength> strength;
    std::optional<icu4x::CollatorAlternateHandling> alternate_handling;
    std::optional<icu4x::CollatorMaxVariable> max_variable;
    std::optional<icu4x::CollatorCaseLevel> case_level;

    inline icu4x::capi::CollatorOptionsV1 AsFFI() const;
    inline static icu4x::CollatorOptionsV1 FromFFI(icu4x::capi::CollatorOptionsV1 c_struct);
};

} // namespace
#endif // ICU4X_CollatorOptionsV1_D_HPP
