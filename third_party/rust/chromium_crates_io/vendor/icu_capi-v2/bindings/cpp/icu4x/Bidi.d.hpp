#ifndef ICU4X_Bidi_D_HPP
#define ICU4X_Bidi_D_HPP

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
namespace capi { struct Bidi; }
class Bidi;
namespace capi { struct BidiInfo; }
class BidiInfo;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct ReorderedIndexMap; }
class ReorderedIndexMap;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct Bidi;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Bidi object, containing loaded bidi data
 *
 * See the [Rust documentation for `BidiClass`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BidiClass.html) for more information.
 */
class Bidi {
public:

  /**
   * Creates a new {@link Bidi} from locale data using compiled data.
   */
  inline static std::unique_ptr<icu4x::Bidi> create();

  /**
   * Creates a new {@link Bidi} from locale data, and a particular data source.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Bidi>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Use the data loaded in this object to process a string and calculate bidi information
   *
   * Takes in a Level for the default level, if it is an invalid value or None it will default to Auto.
   *
   * Returns nothing if `text` is invalid UTF-8.
   *
   * See the [Rust documentation for `new_with_data_source`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/struct.BidiInfo.html#method.new_with_data_source) for more information.
   */
  inline std::unique_ptr<icu4x::BidiInfo> for_text(std::string_view text, std::optional<uint8_t> default_level) const;

  /**
   * Utility function for producing reorderings given a list of levels
   *
   * Produces a map saying which visual index maps to which source index.
   *
   * The levels array must not have values greater than 126 (this is the
   * Bidi maximum explicit depth plus one).
   * Failure to follow this invariant may lead to incorrect results,
   * but is still safe.
   *
   * See the [Rust documentation for `reorder_visual`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/struct.BidiInfo.html#method.reorder_visual) for more information.
   */
  inline std::unique_ptr<icu4x::ReorderedIndexMap> reorder_visual(icu4x::diplomat::span<const uint8_t> levels) const;

  /**
   * Check if a Level returned by `level_at` is an RTL level.
   *
   * Invalid levels (numbers greater than 125) will be assumed LTR
   *
   * See the [Rust documentation for `is_rtl`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/level/struct.Level.html#method.is_rtl) for more information.
   */
  inline static bool level_is_rtl(uint8_t level);

  /**
   * Check if a Level returned by `level_at` is an LTR level.
   *
   * Invalid levels (numbers greater than 125) will be assumed LTR
   *
   * See the [Rust documentation for `is_ltr`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/level/struct.Level.html#method.is_ltr) for more information.
   */
  inline static bool level_is_ltr(uint8_t level);

  /**
   * Get a basic RTL Level value
   *
   * See the [Rust documentation for `rtl`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/level/struct.Level.html#method.rtl) for more information.
   */
  inline static uint8_t level_rtl();

  /**
   * Get a simple LTR Level value
   *
   * See the [Rust documentation for `ltr`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/level/struct.Level.html#method.ltr) for more information.
   */
  inline static uint8_t level_ltr();

    inline const icu4x::capi::Bidi* AsFFI() const;
    inline icu4x::capi::Bidi* AsFFI();
    inline static const icu4x::Bidi* FromFFI(const icu4x::capi::Bidi* ptr);
    inline static icu4x::Bidi* FromFFI(icu4x::capi::Bidi* ptr);
    inline static void operator delete(void* ptr);
private:
    Bidi() = delete;
    Bidi(const icu4x::Bidi&) = delete;
    Bidi(icu4x::Bidi&&) noexcept = delete;
    Bidi operator=(const icu4x::Bidi&) = delete;
    Bidi operator=(icu4x::Bidi&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_Bidi_D_HPP
