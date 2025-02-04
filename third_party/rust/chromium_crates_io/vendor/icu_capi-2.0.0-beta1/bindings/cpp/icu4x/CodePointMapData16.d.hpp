#ifndef icu4x_CodePointMapData16_D_HPP
#define icu4x_CodePointMapData16_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CodePointMapData16; }
class CodePointMapData16;
namespace capi { struct CodePointRangeIterator; }
class CodePointRangeIterator;
namespace capi { struct CodePointSetData; }
class CodePointSetData;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
}


namespace icu4x {
namespace capi {
    struct CodePointMapData16;
} // namespace capi
} // namespace

namespace icu4x {
class CodePointMapData16 {
public:

  inline uint16_t get(char32_t cp) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value(uint16_t value) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value_complemented(uint16_t value) const;

  inline std::unique_ptr<icu4x::CodePointSetData> get_set_for_value(uint16_t value) const;

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData16>, icu4x::DataError> load_script(const icu4x::DataProvider& provider);

  inline const icu4x::capi::CodePointMapData16* AsFFI() const;
  inline icu4x::capi::CodePointMapData16* AsFFI();
  inline static const icu4x::CodePointMapData16* FromFFI(const icu4x::capi::CodePointMapData16* ptr);
  inline static icu4x::CodePointMapData16* FromFFI(icu4x::capi::CodePointMapData16* ptr);
  inline static void operator delete(void* ptr);
private:
  CodePointMapData16() = delete;
  CodePointMapData16(const icu4x::CodePointMapData16&) = delete;
  CodePointMapData16(icu4x::CodePointMapData16&&) noexcept = delete;
  CodePointMapData16 operator=(const icu4x::CodePointMapData16&) = delete;
  CodePointMapData16 operator=(icu4x::CodePointMapData16&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CodePointMapData16_D_HPP
