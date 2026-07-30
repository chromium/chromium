#ifndef ICU4X_Decomposed_D_HPP
#define ICU4X_Decomposed_D_HPP

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
namespace capi {
    struct Decomposed {
      char32_t first;
      char32_t second;
    };

    typedef struct Decomposed_option {union { Decomposed ok; }; bool is_ok; } Decomposed_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * The outcome of non-recursive canonical decomposition of a character.
 * `second` will be NUL when the decomposition expands to a single character
 * (which may or may not be the original one)
 *
 * See the [Rust documentation for `Decomposed`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/enum.Decomposed.html) for more information.
 */
struct Decomposed {
    char32_t first;
    char32_t second;

    inline icu4x::capi::Decomposed AsFFI() const;
    inline static icu4x::Decomposed FromFFI(icu4x::capi::Decomposed c_struct);
};

} // namespace
#endif // ICU4X_Decomposed_D_HPP
