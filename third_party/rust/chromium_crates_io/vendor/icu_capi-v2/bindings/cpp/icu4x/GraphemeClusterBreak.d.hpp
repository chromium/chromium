#ifndef icu4x_GraphemeClusterBreak_D_HPP
#define icu4x_GraphemeClusterBreak_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class GraphemeClusterBreak;
}


namespace icu4x {
namespace capi {
    enum GraphemeClusterBreak {
      GraphemeClusterBreak_Other = 0,
      GraphemeClusterBreak_Control = 1,
      GraphemeClusterBreak_CR = 2,
      GraphemeClusterBreak_Extend = 3,
      GraphemeClusterBreak_L = 4,
      GraphemeClusterBreak_LF = 5,
      GraphemeClusterBreak_LV = 6,
      GraphemeClusterBreak_LVT = 7,
      GraphemeClusterBreak_T = 8,
      GraphemeClusterBreak_V = 9,
      GraphemeClusterBreak_SpacingMark = 10,
      GraphemeClusterBreak_Prepend = 11,
      GraphemeClusterBreak_RegionalIndicator = 12,
      GraphemeClusterBreak_EBase = 13,
      GraphemeClusterBreak_EBaseGAZ = 14,
      GraphemeClusterBreak_EModifier = 15,
      GraphemeClusterBreak_GlueAfterZwj = 16,
      GraphemeClusterBreak_ZWJ = 17,
    };
    
    typedef struct GraphemeClusterBreak_option {union { GraphemeClusterBreak ok; }; bool is_ok; } GraphemeClusterBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
class GraphemeClusterBreak {
public:
  enum Value {
    Other = 0,
    Control = 1,
    CR = 2,
    Extend = 3,
    L = 4,
    LF = 5,
    LV = 6,
    LVT = 7,
    T = 8,
    V = 9,
    SpacingMark = 10,
    Prepend = 11,
    RegionalIndicator = 12,
    EBase = 13,
    EBaseGAZ = 14,
    EModifier = 15,
    GlueAfterZwj = 16,
    ZWJ = 17,
  };

  GraphemeClusterBreak() = default;
  // Implicit conversions between enum and ::Value
  constexpr GraphemeClusterBreak(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline static icu4x::GraphemeClusterBreak for_char(char32_t ch);

  inline uint8_t to_integer_value();

  inline static std::optional<icu4x::GraphemeClusterBreak> from_integer_value(uint8_t other);

  inline icu4x::capi::GraphemeClusterBreak AsFFI() const;
  inline static icu4x::GraphemeClusterBreak FromFFI(icu4x::capi::GraphemeClusterBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_GraphemeClusterBreak_D_HPP
