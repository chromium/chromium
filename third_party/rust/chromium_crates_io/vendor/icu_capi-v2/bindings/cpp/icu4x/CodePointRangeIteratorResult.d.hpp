#ifndef ICU4X_CodePointRangeIteratorResult_D_HPP
#define ICU4X_CodePointRangeIteratorResult_D_HPP

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
    struct CodePointRangeIteratorResult {
      char32_t start;
      char32_t end;
      bool done;
    };

    typedef struct CodePointRangeIteratorResult_option {union { CodePointRangeIteratorResult ok; }; bool is_ok; } CodePointRangeIteratorResult_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * Result of a single iteration of {@link CodePointRangeIterator}.
 * Logically can be considered to be an `Option<RangeInclusive<DiplomatChar>>`,
 *
 * `start` and `end` represent an inclusive range of code points `[start, end]`,
 * and `done` will be true if the iterator has already finished. The last contentful
 * iteration will NOT produce a range `done=true`, in other words `start` and `end` are useful
 * values if and only if `done=false`.
 */
struct CodePointRangeIteratorResult {
    char32_t start;
    char32_t end;
    bool done;

    inline icu4x::capi::CodePointRangeIteratorResult AsFFI() const;
    inline static icu4x::CodePointRangeIteratorResult FromFFI(icu4x::capi::CodePointRangeIteratorResult c_struct);
};

} // namespace
#endif // ICU4X_CodePointRangeIteratorResult_D_HPP
