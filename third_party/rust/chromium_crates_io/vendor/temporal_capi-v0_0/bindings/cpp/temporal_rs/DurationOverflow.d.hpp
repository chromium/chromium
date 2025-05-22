#ifndef temporal_rs_DurationOverflow_D_HPP
#define temporal_rs_DurationOverflow_D_HPP

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
    enum DurationOverflow {
      DurationOverflow_Constrain = 0,
      DurationOverflow_Balance = 1,
    };

    typedef struct DurationOverflow_option {union { DurationOverflow ok; }; bool is_ok; } DurationOverflow_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class DurationOverflow {
public:
  enum Value {
    Constrain = 0,
    Balance = 1,
  };

  DurationOverflow() = default;
  // Implicit conversions between enum and ::Value
  constexpr DurationOverflow(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline temporal_rs::capi::DurationOverflow AsFFI() const;
  inline static temporal_rs::DurationOverflow FromFFI(temporal_rs::capi::DurationOverflow c_enum);
private:
    Value value;
};

} // namespace
#endif // temporal_rs_DurationOverflow_D_HPP
