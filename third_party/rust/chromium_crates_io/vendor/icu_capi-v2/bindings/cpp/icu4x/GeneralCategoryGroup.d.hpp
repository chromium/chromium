#ifndef ICU4X_GeneralCategoryGroup_D_HPP
#define ICU4X_GeneralCategoryGroup_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
struct GeneralCategoryGroup;
class GeneralCategory;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct GeneralCategoryGroup {
      uint32_t mask;
    };

    typedef struct GeneralCategoryGroup_option {union { GeneralCategoryGroup ok; }; bool is_ok; } GeneralCategoryGroup_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * A mask that is capable of representing groups of `General_Category` values.
 *
 * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html) for more information.
 */
struct GeneralCategoryGroup {
    uint32_t mask;

  /**
   * See the [Rust documentation for `contains`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#method.contains) for more information.
   */
  inline bool contains(icu4x::GeneralCategory val) const;

  /**
   * See the [Rust documentation for `complement`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#method.complement) for more information.
   */
  inline icu4x::GeneralCategoryGroup complement() const;

  /**
   * See the [Rust documentation for `all`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#method.all) for more information.
   */
  inline static icu4x::GeneralCategoryGroup all();

  /**
   * See the [Rust documentation for `empty`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#method.empty) for more information.
   */
  inline static icu4x::GeneralCategoryGroup empty();

  /**
   * See the [Rust documentation for `union`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#method.union) for more information.
   */
  inline icu4x::GeneralCategoryGroup union_(icu4x::GeneralCategoryGroup other) const;

  /**
   * See the [Rust documentation for `intersection`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#method.intersection) for more information.
   */
  inline icu4x::GeneralCategoryGroup intersection(icu4x::GeneralCategoryGroup other) const;

  /**
   * See the [Rust documentation for `CasedLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.CasedLetter) for more information.
   */
  inline static icu4x::GeneralCategoryGroup cased_letter();

  /**
   * See the [Rust documentation for `Letter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.Letter) for more information.
   */
  inline static icu4x::GeneralCategoryGroup letter();

  /**
   * See the [Rust documentation for `Mark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.Mark) for more information.
   */
  inline static icu4x::GeneralCategoryGroup mark();

  /**
   * See the [Rust documentation for `Number`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.Number) for more information.
   */
  inline static icu4x::GeneralCategoryGroup number();

  /**
   * See the [Rust documentation for `Other`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.Other) for more information.
   */
  inline static icu4x::GeneralCategoryGroup separator();

  /**
   * See the [Rust documentation for `Letter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.Letter) for more information.
   */
  inline static icu4x::GeneralCategoryGroup other();

  /**
   * See the [Rust documentation for `Punctuation`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.Punctuation) for more information.
   */
  inline static icu4x::GeneralCategoryGroup punctuation();

  /**
   * See the [Rust documentation for `Symbol`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html#associatedconstant.Symbol) for more information.
   */
  inline static icu4x::GeneralCategoryGroup symbol();

    inline icu4x::capi::GeneralCategoryGroup AsFFI() const;
    inline static icu4x::GeneralCategoryGroup FromFFI(icu4x::capi::GeneralCategoryGroup c_struct);
};

} // namespace
#endif // ICU4X_GeneralCategoryGroup_D_HPP
