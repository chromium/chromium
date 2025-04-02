#ifndef icu4x_PluralCategory_D_HPP
#define icu4x_PluralCategory_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class PluralCategory;
}


namespace icu4x {
namespace capi {
    enum PluralCategory {
      PluralCategory_Zero = 0,
      PluralCategory_One = 1,
      PluralCategory_Two = 2,
      PluralCategory_Few = 3,
      PluralCategory_Many = 4,
      PluralCategory_Other = 5,
    };
    
    typedef struct PluralCategory_option {union { PluralCategory ok; }; bool is_ok; } PluralCategory_option;
} // namespace capi
} // namespace

namespace icu4x {
class PluralCategory {
public:
  enum Value {
    Zero = 0,
    One = 1,
    Two = 2,
    Few = 3,
    Many = 4,
    Other = 5,
  };

  PluralCategory() = default;
  // Implicit conversions between enum and ::Value
  constexpr PluralCategory(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline static std::optional<icu4x::PluralCategory> get_for_cldr_string(std::string_view s);

  inline icu4x::capi::PluralCategory AsFFI() const;
  inline static icu4x::PluralCategory FromFFI(icu4x::capi::PluralCategory c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_PluralCategory_D_HPP
