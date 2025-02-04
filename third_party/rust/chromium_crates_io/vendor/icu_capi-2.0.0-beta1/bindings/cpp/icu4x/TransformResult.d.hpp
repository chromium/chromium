#ifndef icu4x_TransformResult_D_HPP
#define icu4x_TransformResult_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum TransformResult {
      TransformResult_Modified = 0,
      TransformResult_Unmodified = 1,
    };
    
    typedef struct TransformResult_option {union { TransformResult ok; }; bool is_ok; } TransformResult_option;
} // namespace capi
} // namespace

namespace icu4x {
class TransformResult {
public:
  enum Value {
    Modified = 0,
    Unmodified = 1,
  };

  TransformResult() = default;
  // Implicit conversions between enum and ::Value
  constexpr TransformResult(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::TransformResult AsFFI() const;
  inline static icu4x::TransformResult FromFFI(icu4x::capi::TransformResult c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_TransformResult_D_HPP
