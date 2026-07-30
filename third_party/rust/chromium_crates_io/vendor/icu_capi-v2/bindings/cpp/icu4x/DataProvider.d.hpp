#ifndef ICU4X_DataProvider_D_HPP
#define ICU4X_DataProvider_D_HPP

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
namespace capi { struct LocaleFallbacker; }
class LocaleFallbacker;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DataProvider;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X data provider, capable of loading ICU4X data keys from some source.
 *
 * Currently the only source supported is loading from "blob" formatted data from a bytes buffer or the file system.
 *
 * If you wish to use ICU4X's builtin "compiled data", use the version of the constructors that do not have `_with_provider`
 * in their names.
 *
 * See the [Rust documentation for `icu_provider`](https://docs.rs/icu_provider/2.2.0/icu_provider/index.html) for more information.
 */
class DataProvider {
public:

  /**
   * Constructs an `FsDataProvider` and returns it as an {@link DataProvider}.
   * Requires the `provider_fs` Cargo feature.
   * Not supported in WASM.
   *
   * See the [Rust documentation for `FsDataProvider`](https://docs.rs/icu_provider_fs/2.2.0/icu_provider_fs/struct.FsDataProvider.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DataProvider>, icu4x::DataError> from_fs(std::string_view path);

  /**
   * Constructs a `BlobDataProvider` and returns it as an {@link DataProvider}.
   *
   * See the [Rust documentation for `try_new_from_static_blob`](https://docs.rs/icu_provider_blob/2.2.0/icu_provider_blob/struct.BlobDataProvider.html#method.try_new_from_static_blob) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DataProvider>, icu4x::DataError> from_byte_slice(icu4x::diplomat::span<const uint8_t> blob);

  /**
   * Creates a provider that tries the current provider and then, if the current provider
   * doesn't support the data key, another provider `other`.
   *
   * This takes ownership of the `other` provider, leaving an empty provider in its place.
   *
   * See the [Rust documentation for `ForkByMarkerProvider`](https://docs.rs/icu_provider_adapters/2.2.0/icu_provider_adapters/fork/type.ForkByMarkerProvider.html) for more information.
   */
  inline icu4x::diplomat::result<std::monostate, icu4x::DataError> fork_by_marker(icu4x::DataProvider& other);

  /**
   * Same as `fork_by_key` but forks by locale instead of key.
   *
   * See the [Rust documentation for `IdentifierNotFoundPredicate`](https://docs.rs/icu_provider_adapters/2.2.0/icu_provider_adapters/fork/predicates/struct.IdentifierNotFoundPredicate.html) for more information.
   */
  inline icu4x::diplomat::result<std::monostate, icu4x::DataError> fork_by_locale(icu4x::DataProvider& other);

  /**
   * See the [Rust documentation for `new`](https://docs.rs/icu_provider_adapters/2.2.0/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html#method.new) for more information.
   *
   * Additional information: [1](https://docs.rs/icu_provider_adapters/2.2.0/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html)
   */
  inline icu4x::diplomat::result<std::monostate, icu4x::DataError> enable_locale_fallback_with(const icu4x::LocaleFallbacker& fallbacker);

    inline const icu4x::capi::DataProvider* AsFFI() const;
    inline icu4x::capi::DataProvider* AsFFI();
    inline static const icu4x::DataProvider* FromFFI(const icu4x::capi::DataProvider* ptr);
    inline static icu4x::DataProvider* FromFFI(icu4x::capi::DataProvider* ptr);
    inline static void operator delete(void* ptr);
private:
    DataProvider() = delete;
    DataProvider(const icu4x::DataProvider&) = delete;
    DataProvider(icu4x::DataProvider&&) noexcept = delete;
    DataProvider operator=(const icu4x::DataProvider&) = delete;
    DataProvider operator=(icu4x::DataProvider&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_DataProvider_D_HPP
