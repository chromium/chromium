#ifndef ICU4X_ComposingNormalizer_D_HPP
#define ICU4X_ComposingNormalizer_D_HPP

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
namespace capi { struct ComposingNormalizer; }
class ComposingNormalizer;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ComposingNormalizer;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `ComposingNormalizer`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizer.html) for more information.
 */
class ComposingNormalizer {
public:

  /**
   * Construct a new `ComposingNormalizer` instance for NFC using compiled data.
   *
   * See the [Rust documentation for `new_nfc`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizer.html#method.new_nfc) for more information.
   */
  inline static std::unique_ptr<icu4x::ComposingNormalizer> create_nfc();

  /**
   * Construct a new `ComposingNormalizer` instance for NFC using a particular data source.
   *
   * See the [Rust documentation for `new_nfc`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizer.html#method.new_nfc) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError> create_nfc_with_provider(const icu4x::DataProvider& provider);

  /**
   * Construct a new `ComposingNormalizer` instance for NFKC using compiled data.
   *
   * See the [Rust documentation for `new_nfkc`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizer.html#method.new_nfkc) for more information.
   */
  inline static std::unique_ptr<icu4x::ComposingNormalizer> create_nfkc();

  /**
   * Construct a new `ComposingNormalizer` instance for NFKC using a particular data source.
   *
   * See the [Rust documentation for `new_nfkc`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizer.html#method.new_nfkc) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError> create_nfkc_with_provider(const icu4x::DataProvider& provider);

  /**
   * Normalize a string
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `normalize_utf8`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizerBorrowed.html#method.normalize_utf8) for more information.
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
   * See the [Rust documentation for `is_normalized_utf8`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizerBorrowed.html#method.is_normalized_utf8) for more information.
   */
  inline bool is_normalized(std::string_view s) const;

  /**
   * Check if a string is normalized
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `is_normalized_utf16`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizerBorrowed.html#method.is_normalized_utf16) for more information.
   */
  inline bool is_normalized16(std::u16string_view s) const;

  /**
   * Return the index a slice of potentially-invalid UTF-8 is normalized up to
   *
   * See the [Rust documentation for `split_normalized_utf8`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizerBorrowed.html#method.split_normalized_utf8) for more information.
   *
   * See the [Rust documentation for `split_normalized`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizerBorrowed.html#method.split_normalized) for more information.
   */
  inline size_t is_normalized_up_to(std::string_view s) const;

  /**
   * Return the index a slice of potentially-invalid UTF-16 is normalized up to
   *
   * See the [Rust documentation for `split_normalized_utf16`](https://docs.rs/icu/2.2.0/icu/normalizer/struct.ComposingNormalizerBorrowed.html#method.split_normalized_utf16) for more information.
   */
  inline size_t is_normalized16_up_to(std::u16string_view s) const;

    inline const icu4x::capi::ComposingNormalizer* AsFFI() const;
    inline icu4x::capi::ComposingNormalizer* AsFFI();
    inline static const icu4x::ComposingNormalizer* FromFFI(const icu4x::capi::ComposingNormalizer* ptr);
    inline static icu4x::ComposingNormalizer* FromFFI(icu4x::capi::ComposingNormalizer* ptr);
    inline static void operator delete(void* ptr);
private:
    ComposingNormalizer() = delete;
    ComposingNormalizer(const icu4x::ComposingNormalizer&) = delete;
    ComposingNormalizer(icu4x::ComposingNormalizer&&) noexcept = delete;
    ComposingNormalizer operator=(const icu4x::ComposingNormalizer&) = delete;
    ComposingNormalizer operator=(icu4x::ComposingNormalizer&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ComposingNormalizer_D_HPP
