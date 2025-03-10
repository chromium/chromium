#ifndef icu4x_JoiningType_D_HPP
#define icu4x_JoiningType_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class JoiningType;
}


namespace icu4x {
namespace capi {
    enum JoiningType {
      JoiningType_NonJoining = 0,
      JoiningType_JoinCausing = 1,
      JoiningType_DualJoining = 2,
      JoiningType_LeftJoining = 3,
      JoiningType_RightJoining = 4,
      JoiningType_Transparent = 5,
    };
    
    typedef struct JoiningType_option {union { JoiningType ok; }; bool is_ok; } JoiningType_option;
} // namespace capi
} // namespace

namespace icu4x {
class JoiningType {
public:
  enum Value {
    NonJoining = 0,
    JoinCausing = 1,
    DualJoining = 2,
    LeftJoining = 3,
    RightJoining = 4,
    Transparent = 5,
  };

  JoiningType() = default;
  // Implicit conversions between enum and ::Value
  constexpr JoiningType(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline static icu4x::JoiningType for_char(char32_t ch);

  inline std::optional<std::string_view> long_name();

  inline std::optional<std::string_view> short_name();

  inline uint8_t to_integer_value();

  inline static std::optional<icu4x::JoiningType> from_integer_value(uint8_t other);

  inline icu4x::capi::JoiningType AsFFI() const;
  inline static icu4x::JoiningType FromFFI(icu4x::capi::JoiningType c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_JoiningType_D_HPP
