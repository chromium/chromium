#ifndef ICU4X_ScriptWithExtensionsBorrowed_D_HPP
#define ICU4X_ScriptWithExtensionsBorrowed_D_HPP

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
namespace capi { struct CodePointSetData; }
class CodePointSetData;
namespace capi { struct ScriptExtensionsSet; }
class ScriptExtensionsSet;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ScriptWithExtensionsBorrowed;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A slightly faster `ScriptWithExtensions` object
 *
 * See the [Rust documentation for `ScriptWithExtensionsBorrowed`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html) for more information.
 */
class ScriptWithExtensionsBorrowed {
public:

  /**
   * Get the Script property value for a code point
   * Get the Script property value for a code point
   *
   * See the [Rust documentation for `get_script_val`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val) for more information.
   */
  inline uint16_t get_script_val(char32_t ch) const;

  /**
   * Get the Script property value for a code point
   *
   * See the [Rust documentation for `get_script_extensions_val`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_val) for more information.
   */
  inline std::unique_ptr<icu4x::ScriptExtensionsSet> get_script_extensions_val(char32_t ch) const;

  /**
   * Check if the `Script_Extensions` property of the given code point covers the given script
   *
   * See the [Rust documentation for `has_script`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script) for more information.
   */
  inline bool has_script(char32_t ch, uint16_t script) const;

  /**
   * Build the `CodePointSetData` corresponding to a codepoints matching a particular script
   * in their `Script_Extensions`
   *
   * See the [Rust documentation for `get_script_extensions_set`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_set) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointSetData> get_script_extensions_set(uint16_t script) const;

    inline const icu4x::capi::ScriptWithExtensionsBorrowed* AsFFI() const;
    inline icu4x::capi::ScriptWithExtensionsBorrowed* AsFFI();
    inline static const icu4x::ScriptWithExtensionsBorrowed* FromFFI(const icu4x::capi::ScriptWithExtensionsBorrowed* ptr);
    inline static icu4x::ScriptWithExtensionsBorrowed* FromFFI(icu4x::capi::ScriptWithExtensionsBorrowed* ptr);
    inline static void operator delete(void* ptr);
private:
    ScriptWithExtensionsBorrowed() = delete;
    ScriptWithExtensionsBorrowed(const icu4x::ScriptWithExtensionsBorrowed&) = delete;
    ScriptWithExtensionsBorrowed(icu4x::ScriptWithExtensionsBorrowed&&) noexcept = delete;
    ScriptWithExtensionsBorrowed operator=(const icu4x::ScriptWithExtensionsBorrowed&) = delete;
    ScriptWithExtensionsBorrowed operator=(icu4x::ScriptWithExtensionsBorrowed&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ScriptWithExtensionsBorrowed_D_HPP
