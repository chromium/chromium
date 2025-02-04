#ifndef icu4x_LanguageDisplay_D_HPP
#define icu4x_LanguageDisplay_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum LanguageDisplay {
      LanguageDisplay_Dialect = 0,
      LanguageDisplay_Standard = 1,
    };
    
    typedef struct LanguageDisplay_option {union { LanguageDisplay ok; }; bool is_ok; } LanguageDisplay_option;
} // namespace capi
} // namespace

namespace icu4x {
class LanguageDisplay {
public:
  enum Value {
    Dialect = 0,
    Standard = 1,
  };

  LanguageDisplay() = default;
  // Implicit conversions between enum and ::Value
  constexpr LanguageDisplay(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::LanguageDisplay AsFFI() const;
  inline static icu4x::LanguageDisplay FromFFI(icu4x::capi::LanguageDisplay c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_LanguageDisplay_D_HPP
