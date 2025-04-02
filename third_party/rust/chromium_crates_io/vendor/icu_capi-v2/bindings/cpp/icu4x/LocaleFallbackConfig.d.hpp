#ifndef icu4x_LocaleFallbackConfig_D_HPP
#define icu4x_LocaleFallbackConfig_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "LocaleFallbackPriority.d.hpp"

namespace icu4x {
class LocaleFallbackPriority;
}


namespace icu4x {
namespace capi {
    struct LocaleFallbackConfig {
      icu4x::capi::LocaleFallbackPriority priority;
    };
    
    typedef struct LocaleFallbackConfig_option {union { LocaleFallbackConfig ok; }; bool is_ok; } LocaleFallbackConfig_option;
} // namespace capi
} // namespace


namespace icu4x {
struct LocaleFallbackConfig {
  icu4x::LocaleFallbackPriority priority;

  inline icu4x::capi::LocaleFallbackConfig AsFFI() const;
  inline static icu4x::LocaleFallbackConfig FromFFI(icu4x::capi::LocaleFallbackConfig c_struct);
};

} // namespace
#endif // icu4x_LocaleFallbackConfig_D_HPP
