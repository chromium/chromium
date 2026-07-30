#ifndef ICU4X_PluralOperands_D_HPP
#define ICU4X_PluralOperands_D_HPP

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
namespace capi { struct Decimal; }
class Decimal;
namespace capi { struct PluralOperands; }
class PluralOperands;
class DecimalParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct PluralOperands;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `PluralOperands`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralOperands.html) for more information.
 */
class PluralOperands {
public:

  /**
   * Construct for a given string representing a number
   *
   * See the [Rust documentation for `from_str`](https://docs.rs/icu/2.2.0/icu/plurals/struct.PluralOperands.html#method.from_str) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::PluralOperands>, icu4x::DecimalParseError> from_string(std::string_view s);

  /**
   * Construct for a given integer
   */
  inline static std::unique_ptr<icu4x::PluralOperands> from(int64_t i);

  /**
   * Construct from a `FixedDecimal`
   *
   * Retains at most 18 digits each from the integer and fraction parts.
   */
  inline static std::unique_ptr<icu4x::PluralOperands> from_fixed_decimal(const icu4x::Decimal& x);

    inline const icu4x::capi::PluralOperands* AsFFI() const;
    inline icu4x::capi::PluralOperands* AsFFI();
    inline static const icu4x::PluralOperands* FromFFI(const icu4x::capi::PluralOperands* ptr);
    inline static icu4x::PluralOperands* FromFFI(icu4x::capi::PluralOperands* ptr);
    inline static void operator delete(void* ptr);
private:
    PluralOperands() = delete;
    PluralOperands(const icu4x::PluralOperands&) = delete;
    PluralOperands(icu4x::PluralOperands&&) noexcept = delete;
    PluralOperands operator=(const icu4x::PluralOperands&) = delete;
    PluralOperands operator=(icu4x::PluralOperands&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_PluralOperands_D_HPP
