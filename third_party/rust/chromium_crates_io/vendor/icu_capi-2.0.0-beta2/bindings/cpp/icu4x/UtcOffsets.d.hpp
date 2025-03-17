#ifndef icu4x_UtcOffsets_D_HPP
#define icu4x_UtcOffsets_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct UtcOffset; }
class UtcOffset;
}


namespace icu4x {
namespace capi {
    struct UtcOffsets {
      icu4x::capi::UtcOffset* standard;
      icu4x::capi::UtcOffset* daylight;
    };
    
    typedef struct UtcOffsets_option {union { UtcOffsets ok; }; bool is_ok; } UtcOffsets_option;
} // namespace capi
} // namespace


namespace icu4x {
struct UtcOffsets {
  std::unique_ptr<icu4x::UtcOffset> standard;
  std::unique_ptr<icu4x::UtcOffset> daylight;

  inline icu4x::capi::UtcOffsets AsFFI() const;
  inline static icu4x::UtcOffsets FromFFI(icu4x::capi::UtcOffsets c_struct);
};

} // namespace
#endif // icu4x_UtcOffsets_D_HPP
