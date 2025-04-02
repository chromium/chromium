#ifndef icu4x_LocaleFallbackPriority_D_HPP
#define icu4x_LocaleFallbackPriority_D_HPP

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
    enum LocaleFallbackPriority {
      LocaleFallbackPriority_Language = 0,
      LocaleFallbackPriority_Region = 1,
    };
    
    typedef struct LocaleFallbackPriority_option {union { LocaleFallbackPriority ok; }; bool is_ok; } LocaleFallbackPriority_option;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleFallbackPriority {
public:
  enum Value {
    Language = 0,
    Region = 1,
  };

  LocaleFallbackPriority() = default;
  // Implicit conversions between enum and ::Value
  constexpr LocaleFallbackPriority(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::LocaleFallbackPriority AsFFI() const;
  inline static icu4x::LocaleFallbackPriority FromFFI(icu4x::capi::LocaleFallbackPriority c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LocaleFallbackPriority_D_HPP
