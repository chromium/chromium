#ifndef ICU4X_BidiInfo_D_HPP
#define ICU4X_BidiInfo_D_HPP

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
namespace capi { struct BidiParagraph; }
class BidiParagraph;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct BidiInfo;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An object containing bidi information for a given string, produced by `for_text()` on `Bidi`
 *
 * See the [Rust documentation for `BidiInfo`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/struct.BidiInfo.html) for more information.
 */
class BidiInfo {
public:

  /**
   * The number of paragraphs contained here
   */
  inline size_t paragraph_count() const;

  /**
   * Get the nth paragraph, returning `None` if out of bounds
   */
  inline std::unique_ptr<icu4x::BidiParagraph> paragraph_at(size_t n) const;

  /**
   * The number of bytes in this full text
   */
  inline size_t size() const;

  /**
   * Get the BIDI level at a particular byte index in the full text.
   * This integer is conceptually a `unicode_bidi::Level`,
   * and can be further inspected using the static methods on Bidi.
   *
   * Returns 0 (equivalent to `Level::ltr()`) on error
   */
  inline uint8_t level_at(size_t pos) const;

    inline const icu4x::capi::BidiInfo* AsFFI() const;
    inline icu4x::capi::BidiInfo* AsFFI();
    inline static const icu4x::BidiInfo* FromFFI(const icu4x::capi::BidiInfo* ptr);
    inline static icu4x::BidiInfo* FromFFI(icu4x::capi::BidiInfo* ptr);
    inline static void operator delete(void* ptr);
private:
    BidiInfo() = delete;
    BidiInfo(const icu4x::BidiInfo&) = delete;
    BidiInfo(icu4x::BidiInfo&&) noexcept = delete;
    BidiInfo operator=(const icu4x::BidiInfo&) = delete;
    BidiInfo operator=(icu4x::BidiInfo&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_BidiInfo_D_HPP
