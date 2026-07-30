#ifndef ICU4X_DisplayNamesOptionsV1_D_HPP
#define ICU4X_DisplayNamesOptionsV1_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DisplayNamesFallback.d.hpp"
#include "DisplayNamesStyle.d.hpp"
#include "LanguageDisplay.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
class DisplayNamesFallback;
class DisplayNamesStyle;
class LanguageDisplay;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DisplayNamesOptionsV1 {
      icu4x::capi::DisplayNamesStyle_option style;
      icu4x::capi::DisplayNamesFallback_option fallback;
      icu4x::capi::LanguageDisplay_option language_display;
    };

    typedef struct DisplayNamesOptionsV1_option {union { DisplayNamesOptionsV1 ok; }; bool is_ok; } DisplayNamesOptionsV1_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 *
 * See the [Rust documentation for `DisplayNamesOptions`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/struct.DisplayNamesOptions.html) for more information.
 */
struct DisplayNamesOptionsV1 {
    std::optional<icu4x::DisplayNamesStyle> style;
    std::optional<icu4x::DisplayNamesFallback> fallback;
    std::optional<icu4x::LanguageDisplay> language_display;

    inline icu4x::capi::DisplayNamesOptionsV1 AsFFI() const;
    inline static icu4x::DisplayNamesOptionsV1 FromFFI(icu4x::capi::DisplayNamesOptionsV1 c_struct);
};

} // namespace
#endif // ICU4X_DisplayNamesOptionsV1_D_HPP
