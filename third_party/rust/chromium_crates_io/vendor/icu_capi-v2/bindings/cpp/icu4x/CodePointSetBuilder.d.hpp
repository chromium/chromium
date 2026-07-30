#ifndef ICU4X_CodePointSetBuilder_D_HPP
#define ICU4X_CodePointSetBuilder_D_HPP

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
namespace capi { struct CodePointSetBuilder; }
class CodePointSetBuilder;
namespace capi { struct CodePointSetData; }
class CodePointSetData;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CodePointSetBuilder;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `CodePointInversionListBuilder`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html) for more information.
 */
class CodePointSetBuilder {
public:

  /**
   * Make a new set builder containing nothing
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::CodePointSetBuilder> create();

  /**
   * Build this into a set
   *
   * This object is repopulated with an empty builder
   *
   * See the [Rust documentation for `build`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.build) for more information.
   */
  inline std::unique_ptr<icu4x::CodePointSetData> build();

  /**
   * Complements this set
   *
   * (Elements in this set are removed and vice versa)
   *
   * See the [Rust documentation for `complement`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement) for more information.
   */
  inline void complement();

  /**
   * Returns whether this set is empty
   *
   * See the [Rust documentation for `is_empty`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.is_empty) for more information.
   */
  inline bool is_empty() const;

  /**
   * Add a single character to the set
   *
   * See the [Rust documentation for `add_char`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_char) for more information.
   */
  inline void add_char(char32_t ch);

  /**
   * Add an inclusive range of characters to the set
   *
   * See the [Rust documentation for `add_range`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_range) for more information.
   */
  inline void add_inclusive_range(char32_t start, char32_t end);

  /**
   * Add all elements that belong to the provided set to the set
   *
   * See the [Rust documentation for `add_set`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_set) for more information.
   */
  inline void add_set(const icu4x::CodePointSetData& data);

  /**
   * Remove a single character to the set
   *
   * See the [Rust documentation for `remove_char`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_char) for more information.
   */
  inline void remove_char(char32_t ch);

  /**
   * Remove an inclusive range of characters from the set
   *
   * See the [Rust documentation for `remove_range`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_range) for more information.
   */
  inline void remove_inclusive_range(char32_t start, char32_t end);

  /**
   * Remove all elements that belong to the provided set from the set
   *
   * See the [Rust documentation for `remove_set`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_set) for more information.
   */
  inline void remove_set(const icu4x::CodePointSetData& data);

  /**
   * Removes all elements from the set except a single character
   *
   * See the [Rust documentation for `retain_char`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_char) for more information.
   */
  inline void retain_char(char32_t ch);

  /**
   * Removes all elements from the set except an inclusive range of characters f
   *
   * See the [Rust documentation for `retain_range`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_range) for more information.
   */
  inline void retain_inclusive_range(char32_t start, char32_t end);

  /**
   * Removes all elements from the set except all elements in the provided set
   *
   * See the [Rust documentation for `retain_set`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_set) for more information.
   */
  inline void retain_set(const icu4x::CodePointSetData& data);

  /**
   * Complement a single character to the set
   *
   * (Characters which are in this set are removed and vice versa)
   *
   * See the [Rust documentation for `complement_char`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_char) for more information.
   */
  inline void complement_char(char32_t ch);

  /**
   * Complement an inclusive range of characters from the set
   *
   * (Characters which are in this set are removed and vice versa)
   *
   * See the [Rust documentation for `complement_range`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_range) for more information.
   */
  inline void complement_inclusive_range(char32_t start, char32_t end);

  /**
   * Complement all elements that belong to the provided set from the set
   *
   * (Characters which are in this set are removed and vice versa)
   *
   * See the [Rust documentation for `complement_set`](https://docs.rs/icu/2.2.0/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_set) for more information.
   */
  inline void complement_set(const icu4x::CodePointSetData& data);

    inline const icu4x::capi::CodePointSetBuilder* AsFFI() const;
    inline icu4x::capi::CodePointSetBuilder* AsFFI();
    inline static const icu4x::CodePointSetBuilder* FromFFI(const icu4x::capi::CodePointSetBuilder* ptr);
    inline static icu4x::CodePointSetBuilder* FromFFI(icu4x::capi::CodePointSetBuilder* ptr);
    inline static void operator delete(void* ptr);
private:
    CodePointSetBuilder() = delete;
    CodePointSetBuilder(const icu4x::CodePointSetBuilder&) = delete;
    CodePointSetBuilder(icu4x::CodePointSetBuilder&&) noexcept = delete;
    CodePointSetBuilder operator=(const icu4x::CodePointSetBuilder&) = delete;
    CodePointSetBuilder operator=(icu4x::CodePointSetBuilder&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CodePointSetBuilder_D_HPP
