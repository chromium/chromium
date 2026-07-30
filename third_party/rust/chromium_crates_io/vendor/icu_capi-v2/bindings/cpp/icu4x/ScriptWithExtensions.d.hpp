#ifndef ICU4X_ScriptWithExtensions_D_HPP
#define ICU4X_ScriptWithExtensions_D_HPP

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
namespace capi { struct CodePointRangeIterator; }
class CodePointRangeIterator;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct ScriptWithExtensions; }
class ScriptWithExtensions;
namespace capi { struct ScriptWithExtensionsBorrowed; }
class ScriptWithExtensionsBorrowed;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ScriptWithExtensions;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X `ScriptWithExtensions` map object, capable of holding a map of codepoints to scriptextensions values
 *
 * See the [Rust documentation for `ScriptWithExtensions`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensions.html) for more information.
 */
class ScriptWithExtensions {
public:

  /**
   * Create a map for the `Script`/`Script_Extensions` properties, using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensions.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::ScriptWithExtensions> create();

  /**
   * Create a map for the `Script`/`Script_Extensions` properties, using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensions.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ScriptWithExtensions>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the Script property value for a code point
   *
   * See the [Rust documentation for `get_script_val`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val) for more information.
   */
  inline uint16_t get_script_val(char32_t ch) const;

  /**
   * Check if the `Script_Extensions` property of the given code point covers the given script
   *
   * See the [Rust documentation for `has_script`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script) for more information.
   */
  inline bool has_script(char32_t ch, uint16_t script) const;

  /**
   * Borrow this object for a slightly faster variant with more operations
   *
   * See the [Rust documentation for `as_borrowed`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensions.html#method.as_borrowed) for more information.
   */
  inline std::unique_ptr<icu4x::ScriptWithExtensionsBorrowed> as_borrowed() const;

  /**
   * Get a list of ranges of code points that contain this script in their `Script_Extensions` values
   *
   * See the [Rust documentation for `get_script_extensions_ranges`](https://docs.rs/icu/2.2.0/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_ranges) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_script(uint16_t script) const;

    inline const icu4x::capi::ScriptWithExtensions* AsFFI() const;
    inline icu4x::capi::ScriptWithExtensions* AsFFI();
    inline static const icu4x::ScriptWithExtensions* FromFFI(const icu4x::capi::ScriptWithExtensions* ptr);
    inline static icu4x::ScriptWithExtensions* FromFFI(icu4x::capi::ScriptWithExtensions* ptr);
    inline static void operator delete(void* ptr);
private:
    ScriptWithExtensions() = delete;
    ScriptWithExtensions(const icu4x::ScriptWithExtensions&) = delete;
    ScriptWithExtensions(icu4x::ScriptWithExtensions&&) noexcept = delete;
    ScriptWithExtensions operator=(const icu4x::ScriptWithExtensions&) = delete;
    ScriptWithExtensions operator=(icu4x::ScriptWithExtensions&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ScriptWithExtensions_D_HPP
