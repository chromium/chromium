#ifndef icu4x_ScriptWithExtensionsBorrowed_D_HPP
#define icu4x_ScriptWithExtensionsBorrowed_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CodePointSetData; }
class CodePointSetData;
namespace capi { struct ScriptExtensionsSet; }
class ScriptExtensionsSet;
}


namespace icu4x {
namespace capi {
    struct ScriptWithExtensionsBorrowed;
} // namespace capi
} // namespace

namespace icu4x {
class ScriptWithExtensionsBorrowed {
public:

  inline uint16_t get_script_val(char32_t ch) const;

  inline std::unique_ptr<icu4x::ScriptExtensionsSet> get_script_extensions_val(char32_t ch) const;

  inline bool has_script(char32_t ch, uint16_t script) const;

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
#endif // icu4x_ScriptWithExtensionsBorrowed_D_HPP
