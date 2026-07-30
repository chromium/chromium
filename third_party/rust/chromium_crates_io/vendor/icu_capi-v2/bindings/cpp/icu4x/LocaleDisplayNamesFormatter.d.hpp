#ifndef ICU4X_LocaleDisplayNamesFormatter_D_HPP
#define ICU4X_LocaleDisplayNamesFormatter_D_HPP

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
namespace capi { struct LocaleDisplayNamesFormatter; }
class LocaleDisplayNamesFormatter;
struct DisplayNamesOptionsV1;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleDisplayNamesFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 *
 * See the [Rust documentation for `LocaleDisplayNamesFormatter`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/multi/struct.LocaleDisplayNamesFormatter.html) for more information.
 */
class LocaleDisplayNamesFormatter {
public:

  /**
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   *
   * Creates a new `LocaleDisplayNamesFormatter` from locale data and an options bag using compiled data.
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/multi/struct.LocaleDisplayNamesFormatter.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleDisplayNamesFormatter>, icu4x::DataError> create_v1(const icu4x::Locale& locale, icu4x::DisplayNamesOptionsV1 options);

  /**
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   *
   * Creates a new `LocaleDisplayNamesFormatter` from locale data and an options bag using a particular data source.
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/struct.LocaleDisplayNamesFormatter.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleDisplayNamesFormatter>, icu4x::DataError> create_v1_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DisplayNamesOptionsV1 options);

  /**
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   *
   * Returns the locale-specific display name of a locale.
   * 🚧 This API is unstable and may experience breaking changes outside major releases.
   *
   * See the [Rust documentation for `of`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/multi/struct.LocaleDisplayNamesFormatter.html#method.of) for more information.
   */
  inline std::string of(const icu4x::Locale& locale) const;
  template<typename W>
  inline void of_write(const icu4x::Locale& locale, W& writeable_output) const;

    inline const icu4x::capi::LocaleDisplayNamesFormatter* AsFFI() const;
    inline icu4x::capi::LocaleDisplayNamesFormatter* AsFFI();
    inline static const icu4x::LocaleDisplayNamesFormatter* FromFFI(const icu4x::capi::LocaleDisplayNamesFormatter* ptr);
    inline static icu4x::LocaleDisplayNamesFormatter* FromFFI(icu4x::capi::LocaleDisplayNamesFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    LocaleDisplayNamesFormatter() = delete;
    LocaleDisplayNamesFormatter(const icu4x::LocaleDisplayNamesFormatter&) = delete;
    LocaleDisplayNamesFormatter(icu4x::LocaleDisplayNamesFormatter&&) noexcept = delete;
    LocaleDisplayNamesFormatter operator=(const icu4x::LocaleDisplayNamesFormatter&) = delete;
    LocaleDisplayNamesFormatter operator=(icu4x::LocaleDisplayNamesFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LocaleDisplayNamesFormatter_D_HPP
