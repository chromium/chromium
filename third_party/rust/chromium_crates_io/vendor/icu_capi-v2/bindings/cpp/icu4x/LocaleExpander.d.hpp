#ifndef ICU4X_LocaleExpander_D_HPP
#define ICU4X_LocaleExpander_D_HPP

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
namespace capi { struct LocaleExpander; }
class LocaleExpander;
class DataError;
class TransformResult;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleExpander;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A locale expander.
 *
 * See the [Rust documentation for `LocaleExpander`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html) for more information.
 */
class LocaleExpander {
public:

  /**
   * Create a new {@link LocaleExpander} using compiled data.
   *
   * See the [Rust documentation for `new_common`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html#method.new_common) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleExpander> create_common();

  /**
   * Create a new {@link LocaleExpander} using a `new_common` data source.
   *
   * See the [Rust documentation for `new_common`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html#method.new_common) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleExpander>, icu4x::DataError> create_common_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a new {@link LocaleExpander} with extended data using compiled data.
   *
   * See the [Rust documentation for `new_extended`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html#method.new_extended) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleExpander> create_extended();

  /**
   * Create a new {@link LocaleExpander} with extended data using a particular data source.
   *
   * See the [Rust documentation for `new_extended`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html#method.new_extended) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleExpander>, icu4x::DataError> create_extended_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `maximize`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html#method.maximize) for more information.
   */
  inline icu4x::TransformResult maximize(icu4x::Locale& locale) const;

  /**
   * See the [Rust documentation for `minimize`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html#method.minimize) for more information.
   */
  inline icu4x::TransformResult minimize(icu4x::Locale& locale) const;

  /**
   * See the [Rust documentation for `minimize_favor_script`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleExpander.html#method.minimize_favor_script) for more information.
   */
  inline icu4x::TransformResult minimize_favor_script(icu4x::Locale& locale) const;

    inline const icu4x::capi::LocaleExpander* AsFFI() const;
    inline icu4x::capi::LocaleExpander* AsFFI();
    inline static const icu4x::LocaleExpander* FromFFI(const icu4x::capi::LocaleExpander* ptr);
    inline static icu4x::LocaleExpander* FromFFI(icu4x::capi::LocaleExpander* ptr);
    inline static void operator delete(void* ptr);
private:
    LocaleExpander() = delete;
    LocaleExpander(const icu4x::LocaleExpander&) = delete;
    LocaleExpander(icu4x::LocaleExpander&&) noexcept = delete;
    LocaleExpander operator=(const icu4x::LocaleExpander&) = delete;
    LocaleExpander operator=(icu4x::LocaleExpander&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LocaleExpander_D_HPP
