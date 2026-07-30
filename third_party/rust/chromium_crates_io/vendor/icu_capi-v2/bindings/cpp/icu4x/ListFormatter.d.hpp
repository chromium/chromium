#ifndef ICU4X_ListFormatter_D_HPP
#define ICU4X_ListFormatter_D_HPP

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
namespace capi { struct ListFormatter; }
class ListFormatter;
namespace capi { struct Locale; }
class Locale;
class DataError;
class ListLength;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct ListFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `ListFormatter`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html) for more information.
 */
class ListFormatter {
public:

  /**
   * Construct a new `ListFormatter` instance for And patterns from compiled data.
   *
   * See the [Rust documentation for `try_new_and`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.try_new_and) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_and_with_length(const icu4x::Locale& locale, icu4x::ListLength length);

  /**
   * Construct a new `ListFormatter` instance for And patterns
   *
   * See the [Rust documentation for `try_new_and`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.try_new_and) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_and_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length);

  /**
   * Construct a new `ListFormatter` instance for And patterns from compiled data.
   *
   * See the [Rust documentation for `try_new_or`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.try_new_or) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_or_with_length(const icu4x::Locale& locale, icu4x::ListLength length);

  /**
   * Construct a new `ListFormatter` instance for And patterns
   *
   * See the [Rust documentation for `try_new_or`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.try_new_or) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_or_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length);

  /**
   * Construct a new `ListFormatter` instance for And patterns from compiled data.
   *
   * See the [Rust documentation for `try_new_unit`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.try_new_unit) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_unit_with_length(const icu4x::Locale& locale, icu4x::ListLength length);

  /**
   * Construct a new `ListFormatter` instance for And patterns
   *
   * See the [Rust documentation for `try_new_unit`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.try_new_unit) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_unit_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length);

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.format) for more information.
   */
  inline std::string format(icu4x::diplomat::span<const diplomat::string_view_for_slice> list) const;
  template<typename W>
  inline void format_write(icu4x::diplomat::span<const diplomat::string_view_for_slice> list, W& writeable_output) const;

  /**
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/list/struct.ListFormatter.html#method.format) for more information.
   */
  inline std::string format16(icu4x::diplomat::span<const diplomat::u16string_view_for_slice> list) const;
  template<typename W>
  inline void format16_write(icu4x::diplomat::span<const diplomat::u16string_view_for_slice> list, W& writeable_output) const;

    inline const icu4x::capi::ListFormatter* AsFFI() const;
    inline icu4x::capi::ListFormatter* AsFFI();
    inline static const icu4x::ListFormatter* FromFFI(const icu4x::capi::ListFormatter* ptr);
    inline static icu4x::ListFormatter* FromFFI(icu4x::capi::ListFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    ListFormatter() = delete;
    ListFormatter(const icu4x::ListFormatter&) = delete;
    ListFormatter(icu4x::ListFormatter&&) noexcept = delete;
    ListFormatter operator=(const icu4x::ListFormatter&) = delete;
    ListFormatter operator=(icu4x::ListFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_ListFormatter_D_HPP
