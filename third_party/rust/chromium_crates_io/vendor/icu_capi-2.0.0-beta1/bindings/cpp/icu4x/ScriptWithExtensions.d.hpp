#ifndef icu4x_ScriptWithExtensions_D_HPP
#define icu4x_ScriptWithExtensions_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

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
}


namespace icu4x {
namespace capi {
    struct ScriptWithExtensions;
} // namespace capi
} // namespace

namespace icu4x {
class ScriptWithExtensions {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::ScriptWithExtensions>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline uint16_t get_script_val(char32_t ch) const;

  inline bool has_script(char32_t ch, uint16_t script) const;

  inline std::unique_ptr<icu4x::ScriptWithExtensionsBorrowed> as_borrowed() const;

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
#endif // icu4x_ScriptWithExtensions_D_HPP
