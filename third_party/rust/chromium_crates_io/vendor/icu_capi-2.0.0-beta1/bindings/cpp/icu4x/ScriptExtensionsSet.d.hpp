#ifndef icu4x_ScriptExtensionsSet_D_HPP
#define icu4x_ScriptExtensionsSet_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct ScriptExtensionsSet;
} // namespace capi
} // namespace

namespace icu4x {
class ScriptExtensionsSet {
public:

  inline bool contains(uint16_t script) const;

  inline size_t count() const;

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
#endif // icu4x_ScriptExtensionsSet_D_HPP
