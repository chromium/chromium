#ifndef ICU4X_LocaleDirectionality_D_HPP
#define ICU4X_LocaleDirectionality_D_HPP

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
namespace capi { struct LocaleDirectionality; }
class LocaleDirectionality;
class DataError;
class LocaleDirection;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct LocaleDirectionality;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `LocaleDirectionality`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html) for more information.
 */
class LocaleDirectionality {
public:

  /**
   * Construct a new `LocaleDirectionality` instance using compiled data.
   *
   * See the [Rust documentation for `new_common`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html#method.new_common) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleDirectionality> create_common();

  /**
   * Construct a new `LocaleDirectionality` instance using a particular data source.
   *
   * See the [Rust documentation for `new_common`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html#method.new_common) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleDirectionality>, icu4x::DataError> create_common_with_provider(const icu4x::DataProvider& provider);

  /**
   * Construct a new `LocaleDirectionality` instance using compiled data.
   *
   * See the [Rust documentation for `new_extended`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html#method.new_extended) for more information.
   */
  inline static std::unique_ptr<icu4x::LocaleDirectionality> create_extended();

  /**
   * Construct a new `LocaleDirectionality` instance using a particular data source.
   *
   * See the [Rust documentation for `new_extended`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html#method.new_extended) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::LocaleDirectionality>, icu4x::DataError> create_extended_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html#method.get) for more information.
   */
  inline icu4x::LocaleDirection operator[](const icu4x::Locale& locale) const;

  /**
   * See the [Rust documentation for `is_left_to_right`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html#method.is_left_to_right) for more information.
   */
  inline bool is_left_to_right(const icu4x::Locale& locale) const;

  /**
   * See the [Rust documentation for `is_right_to_left`](https://docs.rs/icu/2.2.0/icu/locale/struct.LocaleDirectionality.html#method.is_right_to_left) for more information.
   */
  inline bool is_right_to_left(const icu4x::Locale& locale) const;

    inline const icu4x::capi::LocaleDirectionality* AsFFI() const;
    inline icu4x::capi::LocaleDirectionality* AsFFI();
    inline static const icu4x::LocaleDirectionality* FromFFI(const icu4x::capi::LocaleDirectionality* ptr);
    inline static icu4x::LocaleDirectionality* FromFFI(icu4x::capi::LocaleDirectionality* ptr);
    inline static void operator delete(void* ptr);
private:
    LocaleDirectionality() = delete;
    LocaleDirectionality(const icu4x::LocaleDirectionality&) = delete;
    LocaleDirectionality(icu4x::LocaleDirectionality&&) noexcept = delete;
    LocaleDirectionality operator=(const icu4x::LocaleDirectionality&) = delete;
    LocaleDirectionality operator=(icu4x::LocaleDirectionality&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_LocaleDirectionality_D_HPP
