#ifndef ICU4X_CanonicalDecomposition_D_HPP
#define ICU4X_CanonicalDecomposition_D_HPP

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
namespace capi { struct CanonicalDecomposition; }
class CanonicalDecomposition;
namespace capi { struct DataProvider; }
class DataProvider;
struct Decomposed;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CanonicalDecomposition;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * The raw (non-recursive) canonical decomposition operation.
 *
 * Callers should generally use `DecomposingNormalizer` unless they specifically need raw composition operations
 *
 * See the [Rust documentation for `CanonicalDecomposition`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalDecomposition.html) for more information.
 */
class CanonicalDecomposition {
public:

  /**
   * Construct a new `CanonicalDecomposition` instance for NFC using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalDecomposition.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::CanonicalDecomposition> create();

  /**
   * Construct a new `CanonicalDecomposition` instance for NFC using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalDecomposition.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CanonicalDecomposition>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Performs non-recursive canonical decomposition (including for Hangul).
   *
   * See the [Rust documentation for `decompose`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalDecompositionBorrowed.html#method.decompose) for more information.
   */
  inline icu4x::Decomposed decompose(char32_t c) const;

    inline const icu4x::capi::CanonicalDecomposition* AsFFI() const;
    inline icu4x::capi::CanonicalDecomposition* AsFFI();
    inline static const icu4x::CanonicalDecomposition* FromFFI(const icu4x::capi::CanonicalDecomposition* ptr);
    inline static icu4x::CanonicalDecomposition* FromFFI(icu4x::capi::CanonicalDecomposition* ptr);
    inline static void operator delete(void* ptr);
private:
    CanonicalDecomposition() = delete;
    CanonicalDecomposition(const icu4x::CanonicalDecomposition&) = delete;
    CanonicalDecomposition(icu4x::CanonicalDecomposition&&) noexcept = delete;
    CanonicalDecomposition operator=(const icu4x::CanonicalDecomposition&) = delete;
    CanonicalDecomposition operator=(icu4x::CanonicalDecomposition&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CanonicalDecomposition_D_HPP
