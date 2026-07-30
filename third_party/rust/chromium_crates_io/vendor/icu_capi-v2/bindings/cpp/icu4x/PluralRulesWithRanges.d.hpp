#ifndef ICU4X_PluralRulesWithRanges_D_HPP
#define ICU4X_PluralRulesWithRanges_D_HPP

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
namespace capi { struct PluralOperands; }
class PluralOperands;
namespace capi { struct PluralRulesWithRanges; }
class PluralRulesWithRanges;
class DataError;
class PluralCategory;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct PluralRulesWithRanges;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `PluralRulesWithRanges`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRulesWithRanges.html) for more information.
 */
class PluralRulesWithRanges {
public:

  /**
   * construct a {@link PluralRulesWithRanges} for the given locale, for cardinal numbers, using compiled data.
   *
   * See the [Rust documentation for `try_new_cardinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRulesWithRanges.html#method.try_new_cardinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> create_cardinal(const icu4x::Locale& locale);

  /**
   * construct a {@link PluralRulesWithRanges} for the given locale, for cardinal numbers, using a particular data source.
   *
   * See the [Rust documentation for `try_new_cardinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRulesWithRanges.html#method.try_new_cardinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> create_cardinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Construct a {@link PluralRulesWithRanges} for the given locale, for ordinal numbers, using compiled data.
   *
   * See the [Rust documentation for `try_new_ordinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRulesWithRanges.html#method.try_new_ordinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> create_ordinal(const icu4x::Locale& locale);

  /**
   * Construct a {@link PluralRulesWithRanges} for the given locale, for ordinal numbers, using a particular data source.
   *
   * See the [Rust documentation for `try_new_ordinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRulesWithRanges.html#method.try_new_ordinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRulesWithRanges>, icu4x::DataError> create_ordinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Get the category for a given number represented as operands
   *
   * See the [Rust documentation for `category_for_range`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRulesWithRanges.html#method.category_for_range) for more information.
   */
  inline icu4x::PluralCategory category_for_range(const icu4x::PluralOperands& start, const icu4x::PluralOperands& end) const;

    inline const icu4x::capi::PluralRulesWithRanges* AsFFI() const;
    inline icu4x::capi::PluralRulesWithRanges* AsFFI();
    inline static const icu4x::PluralRulesWithRanges* FromFFI(const icu4x::capi::PluralRulesWithRanges* ptr);
    inline static icu4x::PluralRulesWithRanges* FromFFI(icu4x::capi::PluralRulesWithRanges* ptr);
    inline static void operator delete(void* ptr);
private:
    PluralRulesWithRanges() = delete;
    PluralRulesWithRanges(const icu4x::PluralRulesWithRanges&) = delete;
    PluralRulesWithRanges(icu4x::PluralRulesWithRanges&&) noexcept = delete;
    PluralRulesWithRanges operator=(const icu4x::PluralRulesWithRanges&) = delete;
    PluralRulesWithRanges operator=(icu4x::PluralRulesWithRanges&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_PluralRulesWithRanges_D_HPP
