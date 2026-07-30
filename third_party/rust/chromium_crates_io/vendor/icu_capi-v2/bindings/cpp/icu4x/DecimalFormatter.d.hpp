#ifndef ICU4X_DecimalFormatter_D_HPP
#define ICU4X_DecimalFormatter_D_HPP

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
namespace capi { struct Decimal; }
class Decimal;
namespace capi { struct DecimalFormatter; }
class DecimalFormatter;
namespace capi { struct Locale; }
class Locale;
class DataError;
class DecimalGroupingStrategy;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DecimalFormatter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Decimal Format object, capable of formatting a {@link Decimal} as a string.
 *
 * See the [Rust documentation for `DecimalFormatter`](https://docs.rs/icu/2.2.0/icu/decimal/struct.DecimalFormatter.html) for more information.
 */
class DecimalFormatter {
public:

  /**
   * Creates a new {@link DecimalFormatter}, using compiled data
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/decimal/struct.DecimalFormatter.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> create_with_grouping_strategy(const icu4x::Locale& locale, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy);

  /**
   * Creates a new {@link DecimalFormatter}, using a particular data source.
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/decimal/struct.DecimalFormatter.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> create_with_grouping_strategy_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy);

  /**
   * Creates a new {@link DecimalFormatter} from preconstructed locale data.
   *
   * See the [Rust documentation for `DecimalSymbolsV1`](https://docs.rs/icu/2.2.0/icu/decimal/provider/struct.DecimalSymbolsV1.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> create_with_manual_data(std::string_view plus_sign_prefix, std::string_view plus_sign_suffix, std::string_view minus_sign_prefix, std::string_view minus_sign_suffix, std::string_view decimal_separator, std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, icu4x::diplomat::span<const char32_t> digits, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy);

  /**
   * Formats a {@link Decimal} to a string.
   *
   * See the [Rust documentation for `format`](https://docs.rs/icu/2.2.0/icu/decimal/struct.DecimalFormatter.html#method.format) for more information.
   */
  inline std::string format(const icu4x::Decimal& value) const;
  template<typename W>
  inline void format_write(const icu4x::Decimal& value, W& writeable_output) const;

    inline const icu4x::capi::DecimalFormatter* AsFFI() const;
    inline icu4x::capi::DecimalFormatter* AsFFI();
    inline static const icu4x::DecimalFormatter* FromFFI(const icu4x::capi::DecimalFormatter* ptr);
    inline static icu4x::DecimalFormatter* FromFFI(icu4x::capi::DecimalFormatter* ptr);
    inline static void operator delete(void* ptr);
private:
    DecimalFormatter() = delete;
    DecimalFormatter(const icu4x::DecimalFormatter&) = delete;
    DecimalFormatter(icu4x::DecimalFormatter&&) noexcept = delete;
    DecimalFormatter operator=(const icu4x::DecimalFormatter&) = delete;
    DecimalFormatter operator=(icu4x::DecimalFormatter&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_DecimalFormatter_D_HPP
