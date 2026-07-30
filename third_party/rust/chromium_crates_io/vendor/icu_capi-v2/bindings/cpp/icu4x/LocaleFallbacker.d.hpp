#ifndef ICU4X_LocaleFallbacker_D_HPP
#define ICU4X_LocaleFallbacker_D_HPP

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
namespace capi { struct LocaleFallbackerWithConfig; }
class LocaleFallbackerWithConfig;
struct LocaleFallbackConfig;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleFallbacker;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An object that runs the ICU4X locale fallback algorithm.
 *
 * See the [Rust documentation for `LocaleFallbacker`](https://docs.rs/icu_locale/2.2.0/icu_locale/struct.LocaleFallbacker.html) for more information.
 */
class LocaleFallbacker {
public:

  /**
   * Creates a new `LocaleFallbacker` from compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu_locale/2.2.0/icu_locale/struct.LocaleFallbacker.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleFallbacker> create();

  /**
   * Creates a new `LocaleFallbacker` from a data provider.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu_locale/2.2.0/icu_locale/struct.LocaleFallbacker.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleFallbacker>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Creates a new `LocaleFallbacker` without data for limited functionality.
   *
   * See the [Rust documentation for `new_without_data`](https://docs.rs/icu_locale/2.2.0/icu_locale/struct.LocaleFallbacker.html#method.new_without_data) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleFallbacker> without_data();

  /**
   * Associates this `LocaleFallbacker` with configuration options.
   *
   * See the [Rust documentation for `for_config`](https://docs.rs/icu_locale/2.2.0/icu_locale/struct.LocaleFallbacker.html#method.for_config) for more information.
   */
  inline std::unique_ptr<icu4x::LocaleFallbackerWithConfig> for_config(icu4x::LocaleFallbackConfig config) const;

    inline const icu4x::capi::LocaleFallbacker* AsFFI() const;
    inline icu4x::capi::LocaleFallbacker* AsFFI();
    inline static const icu4x::LocaleFallbacker* FromFFI(const icu4x::capi::LocaleFallbacker* ptr);
    inline static icu4x::LocaleFallbacker* FromFFI(icu4x::capi::LocaleFallbacker* ptr);
    inline static void operator delete(void* ptr);
private:
    LocaleFallbacker() = delete;
    LocaleFallbacker(const icu4x::LocaleFallbacker&) = delete;
    LocaleFallbacker(icu4x::LocaleFallbacker&&) noexcept = delete;
    LocaleFallbacker operator=(const icu4x::LocaleFallbacker&) = delete;
    LocaleFallbacker operator=(icu4x::LocaleFallbacker&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LocaleFallbacker_D_HPP
