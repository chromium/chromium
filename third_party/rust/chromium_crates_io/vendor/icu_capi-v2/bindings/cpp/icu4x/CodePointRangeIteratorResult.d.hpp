#ifndef icu4x_CodePointRangeIteratorResult_D_HPP
#define icu4x_CodePointRangeIteratorResult_D_HPP

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
    struct CodePointRangeIteratorResult {
      char32_t start;
      char32_t end;
      bool done;
    };
    
    typedef struct CodePointRangeIteratorResult_option {union { CodePointRangeIteratorResult ok; }; bool is_ok; } CodePointRangeIteratorResult_option;
} // namespace capi
} // namespace


namespace icu4x {
struct CodePointRangeIteratorResult {
  char32_t start;
  char32_t end;
  bool done;

  inline icu4x::capi::CodePointRangeIteratorResult AsFFI() const;
  inline static icu4x::CodePointRangeIteratorResult FromFFI(icu4x::capi::CodePointRangeIteratorResult c_struct);
};

} // namespace
#endif // icu4x_CodePointRangeIteratorResult_D_HPP
