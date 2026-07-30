#ifndef ICU4X_CanonicalComposition_D_HPP
#define ICU4X_CanonicalComposition_D_HPP

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
namespace capi { struct CanonicalComposition; }
class CanonicalComposition;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CanonicalComposition;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * The raw canonical composition operation.
 *
 * Callers should generally use `ComposingNormalizer` unless they specifically need raw composition operations
 *
 * See the [Rust documentation for `CanonicalComposition`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalComposition.html) for more information.
 */
class CanonicalComposition {
public:

  /**
   * Construct a new `CanonicalComposition` instance for NFC using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalComposition.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::CanonicalComposition> create();

  /**
   * Construct a new `CanonicalComposition` instance for NFC using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalComposition.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CanonicalComposition>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Performs canonical composition (including Hangul) on a pair of characters
   * or returns NUL if these characters don’t compose. Composition exclusions are taken into account.
   *
   * See the [Rust documentation for `compose`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalCompositionBorrowed.html#method.compose) for more information.
   */
  inline char32_t compose(char32_t starter, char32_t second) const;

    inline const icu4x::capi::CanonicalComposition* AsFFI() const;
    inline icu4x::capi::CanonicalComposition* AsFFI();
    inline static const icu4x::CanonicalComposition* FromFFI(const icu4x::capi::CanonicalComposition* ptr);
    inline static icu4x::CanonicalComposition* FromFFI(icu4x::capi::CanonicalComposition* ptr);
    inline static void operator delete(void* ptr);
private:
    CanonicalComposition() = delete;
    CanonicalComposition(const icu4x::CanonicalComposition&) = delete;
    CanonicalComposition(icu4x::CanonicalComposition&&) noexcept = delete;
    CanonicalComposition operator=(const icu4x::CanonicalComposition&) = delete;
    CanonicalComposition operator=(icu4x::CanonicalComposition&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CanonicalComposition_D_HPP
