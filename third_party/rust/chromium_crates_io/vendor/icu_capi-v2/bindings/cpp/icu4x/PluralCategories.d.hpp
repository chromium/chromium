#ifndef icu4x_PluralCategories_D_HPP
#define icu4x_PluralCategories_D_HPP

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
    struct PluralCategories {
      bool zero;
      bool one;
      bool two;
      bool few;
      bool many;
      bool other;
    };
    
    typedef struct PluralCategories_option {union { PluralCategories ok; }; bool is_ok; } PluralCategories_option;
} // namespace capi
} // namespace


namespace icu4x {
struct PluralCategories {
  bool zero;
  bool one;
  bool two;
  bool few;
  bool many;
  bool other;

  inline icu4x::capi::PluralCategories AsFFI() const;
  inline static icu4x::PluralCategories FromFFI(icu4x::capi::PluralCategories c_struct);
};

} // namespace
#endif // icu4x_PluralCategories_D_HPP
