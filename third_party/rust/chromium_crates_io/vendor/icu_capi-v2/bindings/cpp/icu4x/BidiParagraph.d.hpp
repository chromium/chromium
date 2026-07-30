#ifndef ICU4X_BidiParagraph_D_HPP
#define ICU4X_BidiParagraph_D_HPP

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
class BidiDirection;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct BidiParagraph;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Bidi information for a single processed paragraph
 */
class BidiParagraph {
public:

  /**
   * Given a paragraph index `n` within the surrounding text, this sets this
   * object to the paragraph at that index. Returns nothing when out of bounds.
   *
   * This is equivalent to calling `paragraph_at()` on `BidiInfo` but doesn't
   * create a new object
   */
  inline bool set_paragraph_in_text(size_t n);

  /**
   * The primary direction of this paragraph
   *
   * See the [Rust documentation for `level_at`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/struct.Paragraph.html#method.level_at) for more information.
   */
  inline icu4x::BidiDirection direction() const;

  /**
   * The number of bytes in this paragraph
   *
   * See the [Rust documentation for `len`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/struct.ParagraphInfo.html#method.len) for more information.
   */
  inline size_t size() const;

  /**
   * The start index of this paragraph within the source text
   */
  inline size_t range_start() const;

  /**
   * The end index of this paragraph within the source text
   */
  inline size_t range_end() const;

  /**
   * Reorder a line based on display order. The ranges are specified relative to the source text and must be contained
   * within this paragraph's range.
   *
   * See the [Rust documentation for `level_at`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/struct.Paragraph.html#method.level_at) for more information.
   */
  inline std::optional<std::string> reorder_line(size_t range_start, size_t range_end) const;
  template<typename W>
  inline std::optional<std::monostate> reorder_line_write(size_t range_start, size_t range_end, W& writeable_output) const;

  /**
   * Get the BIDI level at a particular byte index in this paragraph.
   * This integer is conceptually a `unicode_bidi::Level`,
   * and can be further inspected using the static methods on Bidi.
   *
   * Returns 0 (equivalent to `Level::ltr()`) on error
   *
   * See the [Rust documentation for `level_at`](https://docs.rs/unicode_bidi/0.3.11/unicode_bidi/struct.Paragraph.html#method.level_at) for more information.
   */
  inline uint8_t level_at(size_t pos) const;

    inline const icu4x::capi::BidiParagraph* AsFFI() const;
    inline icu4x::capi::BidiParagraph* AsFFI();
    inline static const icu4x::BidiParagraph* FromFFI(const icu4x::capi::BidiParagraph* ptr);
    inline static icu4x::BidiParagraph* FromFFI(icu4x::capi::BidiParagraph* ptr);
    inline static void operator delete(void* ptr);
private:
    BidiParagraph() = delete;
    BidiParagraph(const icu4x::BidiParagraph&) = delete;
    BidiParagraph(icu4x::BidiParagraph&&) noexcept = delete;
    BidiParagraph operator=(const icu4x::BidiParagraph&) = delete;
    BidiParagraph operator=(icu4x::BidiParagraph&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_BidiParagraph_D_HPP
