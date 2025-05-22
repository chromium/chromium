#ifndef temporal_rs_I128Nanoseconds_D_HPP
#define temporal_rs_I128Nanoseconds_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    struct I128Nanoseconds {
      int64_t high;
      uint64_t low;
    };

    typedef struct I128Nanoseconds_option {union { I128Nanoseconds ok; }; bool is_ok; } I128Nanoseconds_option;
} // namespace capi
} // namespace


namespace temporal_rs {
/**
 * For portability, we use two i64s instead of an i128.
 * The sign is extracted first before
 * appending the high/low segments to each other.
 *
 * This could potentially instead be a bit-by-bit split, or something else
 */
struct I128Nanoseconds {
  int64_t high;
  uint64_t low;

  inline temporal_rs::capi::I128Nanoseconds AsFFI() const;
  inline static temporal_rs::I128Nanoseconds FromFFI(temporal_rs::capi::I128Nanoseconds c_struct);
};

} // namespace
#endif // temporal_rs_I128Nanoseconds_D_HPP
