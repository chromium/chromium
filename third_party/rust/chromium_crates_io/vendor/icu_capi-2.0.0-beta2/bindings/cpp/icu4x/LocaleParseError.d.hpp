#ifndef icu4x_LocaleParseError_D_HPP
#define icu4x_LocaleParseError_D_HPP

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
    enum LocaleParseError {
      LocaleParseError_Unknown = 0,
      LocaleParseError_Language = 1,
      LocaleParseError_Subtag = 2,
      LocaleParseError_Extension = 3,
    };
    
    typedef struct LocaleParseError_option {union { LocaleParseError ok; }; bool is_ok; } LocaleParseError_option;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleParseError {
public:
  enum Value {
    Unknown = 0,
    Language = 1,
    Subtag = 2,
    Extension = 3,
  };

  LocaleParseError() = default;
  // Implicit conversions between enum and ::Value
  constexpr LocaleParseError(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::LocaleParseError AsFFI() const;
  inline static icu4x::LocaleParseError FromFFI(icu4x::capi::LocaleParseError c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LocaleParseError_D_HPP
