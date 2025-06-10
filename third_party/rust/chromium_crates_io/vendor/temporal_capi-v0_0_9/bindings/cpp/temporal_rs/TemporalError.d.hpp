#ifndef temporal_rs_TemporalError_D_HPP
#define temporal_rs_TemporalError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "ErrorKind.d.hpp"

namespace temporal_rs {
class ErrorKind;
}


namespace temporal_rs {
namespace capi {
    struct TemporalError {
      temporal_rs::capi::ErrorKind kind;
    };

    typedef struct TemporalError_option {union { TemporalError ok; }; bool is_ok; } TemporalError_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct TemporalError {
  temporal_rs::ErrorKind kind;

  inline temporal_rs::capi::TemporalError AsFFI() const;
  inline static temporal_rs::TemporalError FromFFI(temporal_rs::capi::TemporalError c_struct);
};

} // namespace
#endif // temporal_rs_TemporalError_D_HPP
