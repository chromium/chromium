#ifndef icu4x_LineBreakOptionsV2_D_HPP
#define icu4x_LineBreakOptionsV2_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "LineBreakStrictness.d.hpp"
#include "LineBreakWordOption.d.hpp"

namespace icu4x {
class LineBreakStrictness;
class LineBreakWordOption;
}


namespace icu4x {
namespace capi {
    struct LineBreakOptionsV2 {
      icu4x::capi::LineBreakStrictness_option strictness;
      icu4x::capi::LineBreakWordOption_option word_option;
    };
    
    typedef struct LineBreakOptionsV2_option {union { LineBreakOptionsV2 ok; }; bool is_ok; } LineBreakOptionsV2_option;
} // namespace capi
} // namespace


namespace icu4x {
struct LineBreakOptionsV2 {
  std::optional<icu4x::LineBreakStrictness> strictness;
  std::optional<icu4x::LineBreakWordOption> word_option;

  inline icu4x::capi::LineBreakOptionsV2 AsFFI() const;
  inline static icu4x::LineBreakOptionsV2 FromFFI(icu4x::capi::LineBreakOptionsV2 c_struct);
};

} // namespace
#endif // icu4x_LineBreakOptionsV2_D_HPP
