#ifndef ICU4X_ScriptExtensionsSet_D_HPP
#define ICU4X_ScriptExtensionsSet_D_HPP

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
    struct ScriptExtensionsSet;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An object that represents the `Script_Extensions` property for a single character
 *
 * See the [Rust documentation for `ScriptExtensionsSet`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptExtensionsSet.html) for more information.
 */
class ScriptExtensionsSet {
public:

  /**
   * Check if the `Script_Extensions` property of the given code point covers the given script
   *
   * See the [Rust documentation for `contains`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptExtensionsSet.html#method.contains) for more information.
   */
  inline bool contains(uint16_t script) const;

  /**
   * Get the number of scripts contained in here
   *
   * See the [Rust documentation for `iter`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter) for more information.
   */
  inline size_t count() const;

  /**
   * Get script at index
   *
   * See the [Rust documentation for `iter`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter) for more information.
   */
  inline std::optional<uint16_t> script_at(size_t index) const;

    inline const icu4x::capi::ScriptExtensionsSet* AsFFI() const;
    inline icu4x::capi::ScriptExtensionsSet* AsFFI();
    inline static const icu4x::ScriptExtensionsSet* FromFFI(const icu4x::capi::ScriptExtensionsSet* ptr);
    inline static icu4x::ScriptExtensionsSet* FromFFI(icu4x::capi::ScriptExtensionsSet* ptr);
    inline static void operator delete(void* ptr);
private:
    ScriptExtensionsSet() = delete;
    ScriptExtensionsSet(const icu4x::ScriptExtensionsSet&) = delete;
    ScriptExtensionsSet(icu4x::ScriptExtensionsSet&&) noexcept = delete;
    ScriptExtensionsSet operator=(const icu4x::ScriptExtensionsSet&) = delete;
    ScriptExtensionsSet operator=(icu4x::ScriptExtensionsSet&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ScriptExtensionsSet_D_HPP
