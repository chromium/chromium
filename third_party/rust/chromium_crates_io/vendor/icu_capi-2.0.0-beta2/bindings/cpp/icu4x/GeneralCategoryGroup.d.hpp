#ifndef icu4x_GeneralCategoryGroup_D_HPP
#define icu4x_GeneralCategoryGroup_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
struct GeneralCategoryGroup;
class GeneralCategory;
}


namespace icu4x {
namespace capi {
    struct GeneralCategoryGroup {
      uint32_t mask;
    };
    
    typedef struct GeneralCategoryGroup_option {union { GeneralCategoryGroup ok; }; bool is_ok; } GeneralCategoryGroup_option;
} // namespace capi
} // namespace


namespace icu4x {
struct GeneralCategoryGroup {
  uint32_t mask;

  inline bool contains(icu4x::GeneralCategory val);

  inline icu4x::GeneralCategoryGroup complement();

  inline static icu4x::GeneralCategoryGroup all();

  inline static icu4x::GeneralCategoryGroup empty();

  inline icu4x::GeneralCategoryGroup union_(icu4x::GeneralCategoryGroup other);

  inline icu4x::GeneralCategoryGroup intersection(icu4x::GeneralCategoryGroup other);

  inline static icu4x::GeneralCategoryGroup cased_letter();

  inline static icu4x::GeneralCategoryGroup letter();

  inline static icu4x::GeneralCategoryGroup mark();

  inline static icu4x::GeneralCategoryGroup number();

  inline static icu4x::GeneralCategoryGroup separator();

  inline static icu4x::GeneralCategoryGroup other();

  inline static icu4x::GeneralCategoryGroup punctuation();

  inline static icu4x::GeneralCategoryGroup symbol();

  inline icu4x::capi::GeneralCategoryGroup AsFFI() const;
  inline static icu4x::GeneralCategoryGroup FromFFI(icu4x::capi::GeneralCategoryGroup c_struct);
};

} // namespace
#endif // icu4x_GeneralCategoryGroup_D_HPP
