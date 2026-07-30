#ifndef ICU4X_LocaleCanonicalizer_D_HPP
#define ICU4X_LocaleCanonicalizer_D_HPP

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
namespace capi { struct Locale; }
class Locale;
namespace capi { struct LocaleCanonicalizer; }
class LocaleCanonicalizer;
class DataError;
class TransformResult;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleCanonicalizer;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A locale canonicalizer.
 *
 * See the [Rust documentation for `LocaleCanonicalizer`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleCanonicalizer.html) for more information.
 */
class LocaleCanonicalizer {
public:

  /**
   * Create a new {@link LocaleCanonicalizer} using compiled data.
   *
   * See the [Rust documentation for `new_common`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleCanonicalizer.html#method.new_common) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleCanonicalizer> create_common();

  /**
   * Create a new {@link LocaleCanonicalizer}.
   *
   * See the [Rust documentation for `new_common`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleCanonicalizer.html#method.new_common) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError> create_common_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a new {@link LocaleCanonicalizer} with extended data using compiled data.
   *
   * See the [Rust documentation for `new_extended`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleCanonicalizer.html#method.new_extended) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleCanonicalizer> create_extended();

  /**
   * Create a new {@link LocaleCanonicalizer} with extended data.
   *
   * See the [Rust documentation for `new_extended`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleCanonicalizer.html#method.new_extended) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError> create_extended_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `canonicalize`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleCanonicalizer.html#method.canonicalize) for more information.
   */
  inline icu4x::TransformResult canonicalize(icu4x::Locale& locale) const;

    inline const icu4x::capi::LocaleCanonicalizer* AsFFI() const;
    inline icu4x::capi::LocaleCanonicalizer* AsFFI();
    inline static const icu4x::LocaleCanonicalizer* FromFFI(const icu4x::capi::LocaleCanonicalizer* ptr);
    inline static icu4x::LocaleCanonicalizer* FromFFI(icu4x::capi::LocaleCanonicalizer* ptr);
    inline static void operator delete(void* ptr);
private:
    LocaleCanonicalizer() = delete;
    LocaleCanonicalizer(const icu4x::LocaleCanonicalizer&) = delete;
    LocaleCanonicalizer(icu4x::LocaleCanonicalizer&&) noexcept = delete;
    LocaleCanonicalizer operator=(const icu4x::LocaleCanonicalizer&) = delete;
    LocaleCanonicalizer operator=(icu4x::LocaleCanonicalizer&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LocaleCanonicalizer_D_HPP
