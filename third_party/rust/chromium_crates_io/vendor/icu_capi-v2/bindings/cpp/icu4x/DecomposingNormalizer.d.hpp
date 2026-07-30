#ifndef ICU4X_DecomposingNormalizer_D_HPP
#define ICU4X_DecomposingNormalizer_D_HPP

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
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct DecomposingNormalizer; }
class DecomposingNormalizer;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DecomposingNormalizer;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `DecomposingNormalizer`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizer.html) for more information.
 */
class DecomposingNormalizer {
public:

  /**
   * Construct a new `DecomposingNormalizer` instance for NFD using compiled data.
   *
   * See the [Rust documentation for `new_nfd`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizer.html#method.new_nfd) for more information.
   */
  inline static std::unique_ptr<icu4x::DecomposingNormalizer> create_nfd();

  /**
   * Construct a new `DecomposingNormalizer` instance for NFD using a particular data source.
   *
   * See the [Rust documentation for `new_nfd`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizer.html#method.new_nfd) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DecomposingNormalizer>, icu4x::DataError> create_nfd_with_provider(const icu4x::DataProvider& provider);

  /**
   * Construct a new `DecomposingNormalizer` instance for NFKD using compiled data.
   *
   * See the [Rust documentation for `new_nfkd`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizer.html#method.new_nfkd) for more information.
   */
  inline static std::unique_ptr<icu4x::DecomposingNormalizer> create_nfkd();

  /**
   * Construct a new `DecomposingNormalizer` instance for NFKD using a particular data source.
   *
   * See the [Rust documentation for `new_nfkd`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizer.html#method.new_nfkd) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DecomposingNormalizer>, icu4x::DataError> create_nfkd_with_provider(const icu4x::DataProvider& provider);

  /**
   * Normalize a string
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `normalize_utf8`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizerBorrowed.html#method.normalize_utf8) for more information.
   */
  inline std::string normalize(std::string_view s) const;
  template<typename W>
  inline void normalize_write(std::string_view s, W& writeable_output) const;

  /**
   * Check if a string is normalized
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `is_normalized_utf8`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizerBorrowed.html#method.is_normalized_utf8) for more information.
   */
  inline bool is_normalized(std::string_view s) const;

  /**
   * Check if a string is normalized
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `is_normalized_utf16`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizerBorrowed.html#method.is_normalized_utf16) for more information.
   */
  inline bool is_normalized16(std::u16string_view s) const;

  /**
   * Return the index a slice of potentially-invalid UTF-8 is normalized up to
   *
   * See the [Rust documentation for `split_normalized_utf8`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizerBorrowed.html#method.split_normalized_utf8) for more information.
   *
   * See the [Rust documentation for `split_normalized`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizerBorrowed.html#method.split_normalized) for more information.
   */
  inline size_t is_normalized_up_to(std::string_view s) const;

  /**
   * Return the index a slice of potentially-invalid UTF-16 is normalized up to
   *
   * See the [Rust documentation for `split_normalized_utf16`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.DecomposingNormalizerBorrowed.html#method.split_normalized_utf16) for more information.
   */
  inline size_t is_normalized16_up_to(std::u16string_view s) const;

    inline const icu4x::capi::DecomposingNormalizer* AsFFI() const;
    inline icu4x::capi::DecomposingNormalizer* AsFFI();
    inline static const icu4x::DecomposingNormalizer* FromFFI(const icu4x::capi::DecomposingNormalizer* ptr);
    inline static icu4x::DecomposingNormalizer* FromFFI(icu4x::capi::DecomposingNormalizer* ptr);
    inline static void operator delete(void* ptr);
private:
    DecomposingNormalizer() = delete;
    DecomposingNormalizer(const icu4x::DecomposingNormalizer&) = delete;
    DecomposingNormalizer(icu4x::DecomposingNormalizer&&) noexcept = delete;
    DecomposingNormalizer operator=(const icu4x::DecomposingNormalizer&) = delete;
    DecomposingNormalizer operator=(icu4x::DecomposingNormalizer&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_DecomposingNormalizer_D_HPP
