#ifndef icu4x_EastAsianWidth_D_HPP
#define icu4x_EastAsianWidth_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class EastAsianWidth;
}


namespace icu4x {
namespace capi {
    enum EastAsianWidth {
      EastAsianWidth_Neutral = 0,
      EastAsianWidth_Ambiguous = 1,
      EastAsianWidth_Halfwidth = 2,
      EastAsianWidth_Fullwidth = 3,
      EastAsianWidth_Narrow = 4,
      EastAsianWidth_Wide = 5,
    };
    
    typedef struct EastAsianWidth_option {union { EastAsianWidth ok; }; bool is_ok; } EastAsianWidth_option;
} // namespace capi
} // namespace

namespace icu4x {
class EastAsianWidth {
public:
  enum Value {
    Neutral = 0,
    Ambiguous = 1,
    Halfwidth = 2,
    Fullwidth = 3,
    Narrow = 4,
    Wide = 5,
  };

  EastAsianWidth() = default;
  // Implicit conversions between enum and ::Value
  constexpr EastAsianWidth(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline uint8_t to_integer();

  inline static std::optional<icu4x::EastAsianWidth> from_integer(uint8_t other);

  inline icu4x::capi::EastAsianWidth AsFFI() const;
  inline static icu4x::EastAsianWidth FromFFI(icu4x::capi::EastAsianWidth c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_EastAsianWidth_D_HPP
