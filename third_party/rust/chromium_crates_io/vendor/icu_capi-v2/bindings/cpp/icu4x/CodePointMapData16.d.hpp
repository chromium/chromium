#ifndef ICU4X_CodePointMapData16_D_HPP
#define ICU4X_CodePointMapData16_D_HPP

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
namespace capi { struct CodePointMapData16; }
class CodePointMapData16;
namespace capi { struct CodePointRangeIterator; }
class CodePointRangeIterator;
namespace capi { struct CodePointSetData; }
class CodePointSetData;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CodePointMapData16;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.
 *
 * For properties whose values fit into 16 bits.
 *
 * See the [Rust documentation for `properties`](https://docs.rs/icu/2.2.0/icu/properties/index.html) for more information.
 *
 * See the [Rust documentation for `CodePointMapData`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapData.html) for more information.
 *
 * See the [Rust documentation for `CodePointMapDataBorrowed`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html) for more information.
 */
class CodePointMapData16 {
public:

  /**
   * Gets the value for a code point.
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.get) for more information.
   */
  inline uint16_t operator[](char32_t cp) const;

  /**
   * Produces an iterator over ranges of code points that map to `value`
   *
   * See the [Rust documentation for `iter_ranges_for_value`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value(uint16_t value) const;

  /**
   * Produces an iterator over ranges of code points that do not map to `value`
   *
   * See the [Rust documentation for `iter_ranges_for_value_complemented`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value_complemented) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value_complemented(uint16_t value) const;

  /**
   * Gets a {@link CodePointSetData} representing all entries in this map that map to the given value
   *
   * See the [Rust documentation for `get_set_for_value`](https://docs.rs/icu/2.2.0/icu/properties/struct.CodePointMapDataBorrowed.html#method.get_set_for_value) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointSetData> get_set_for_value(uint16_t value) const;

  /**
   * Create a map for the `Script` property, using compiled data.
   *
   * See the [Rust documentation for `Script`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Script.html) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointMapData16> create_script();

  /**
   * Create a map for the `Script` property, using a particular data source.
   *
   * See the [Rust documentation for `Script`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.Script.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData16>, icu4x::DataError> create_script_with_provider(const icu4x::DataProvider& provider);

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
#endif // ICU4X_CodePointMapData16_D_HPP
