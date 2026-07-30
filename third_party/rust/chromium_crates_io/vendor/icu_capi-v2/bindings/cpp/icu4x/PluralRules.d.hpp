#ifndef ICU4X_PluralRules_D_HPP
#define ICU4X_PluralRules_D_HPP

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
namespace capi { struct PluralRules; }
class PluralRules;
struct PluralCategories;
class DataError;
class PluralCategory;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct PluralRules;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `PluralRules`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRules.html) for more information.
 */
class PluralRules {
public:

  /**
   * Construct an {@link PluralRules} for the given locale, for cardinal numbers, using compiled data.
   *
   * See the [Rust documentation for `try_new_cardinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRules.html#method.try_new_cardinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_cardinal(const icu4x::Locale& locale);

  /**
   * Construct an {@link PluralRules} for the given locale, for cardinal numbers, using a particular data source.
   *
   * See the [Rust documentation for `try_new_cardinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRules.html#method.try_new_cardinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_cardinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Construct an {@link PluralRules} for the given locale, for ordinal numbers, using compiled data.
   *
   * See the [Rust documentation for `try_new_ordinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRules.html#method.try_new_ordinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_ordinal(const icu4x::Locale& locale);

  /**
   * Construct an {@link PluralRules} for the given locale, for ordinal numbers, using a particular data source.
   *
   * See the [Rust documentation for `try_new_ordinal`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRules.html#method.try_new_ordinal) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_ordinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Get the category for a given number represented as operands
   *
   * See the [Rust documentation for `category_for`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRules.html#method.category_for) for more information.
   */
  inline icu4x::PluralCategory category_for(const icu4x::PluralOperands& op) const;

  /**
   * Get all of the categories needed in the current locale
   *
   * See the [Rust documentation for `categories`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralRules.html#method.categories) for more information.
   */
  inline icu4x::PluralCategories categories() const;

    inline const icu4x::capi::PluralRules* AsFFI() const;
    inline icu4x::capi::PluralRules* AsFFI();
    inline static const icu4x::PluralRules* FromFFI(const icu4x::capi::PluralRules* ptr);
    inline static icu4x::PluralRules* FromFFI(icu4x::capi::PluralRules* ptr);
    inline static void operator delete(void* ptr);
private:
    PluralRules() = delete;
    PluralRules(const icu4x::PluralRules&) = delete;
    PluralRules(icu4x::PluralRules&&) noexcept = delete;
    PluralRules operator=(const icu4x::PluralRules&) = delete;
    PluralRules operator=(icu4x::PluralRules&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_PluralRules_D_HPP
