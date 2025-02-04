#ifndef icu4x_Decomposed_D_HPP
#define icu4x_Decomposed_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


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
struct Decomposed {
  char32_t first;
  char32_t second;

  inline icu4x::capi::Decomposed AsFFI() const;
  inline static icu4x::Decomposed FromFFI(icu4x::capi::Decomposed c_struct);
};

} // namespace
#endif // icu4x_Decomposed_D_HPP
