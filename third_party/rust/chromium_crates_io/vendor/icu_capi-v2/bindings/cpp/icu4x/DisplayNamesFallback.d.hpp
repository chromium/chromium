#ifndef icu4x_DisplayNamesFallback_D_HPP
#define icu4x_DisplayNamesFallback_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum DisplayNamesFallback {
      DisplayNamesFallback_Code = 0,
      DisplayNamesFallback_None = 1,
    };
    
    typedef struct DisplayNamesFallback_option {union { DisplayNamesFallback ok; }; bool is_ok; } DisplayNamesFallback_option;
} // namespace capi
} // namespace

namespace icu4x {
class DisplayNamesFallback {
public:
  enum Value {
    Code = 0,
    None = 1,
  };

  DisplayNamesFallback() = default;
  // Implicit conversions between enum and ::Value
  constexpr DisplayNamesFallback(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::DisplayNamesFallback AsFFI() const;
  inline static icu4x::DisplayNamesFallback FromFFI(icu4x::capi::DisplayNamesFallback c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_DisplayNamesFallback_D_HPP
