#ifndef ICU4X_PluralCategory_D_HPP
#define ICU4X_PluralCategory_D_HPP

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
class PluralCategory;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum PluralCategory {
      PluralCategory_Zero = 0,
      PluralCategory_One = 1,
      PluralCategory_Two = 2,
      PluralCategory_Few = 3,
      PluralCategory_Many = 4,
      PluralCategory_Other = 5,
    };

    typedef struct PluralCategory_option {union { PluralCategory ok; }; bool is_ok; } PluralCategory_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `PluralCategory`](https://docs.rs/icu/2.2.0/icu/plurals/enum.PluralCategory.html) for more information.
 */
class PluralCategory {
public:
    enum Value {
        Zero = 0,
        One = 1,
        Two = 2,
        Few = 3,
        Many = 4,
        Other = 5,
    };

    PluralCategory(): value(Value::Other) {}

    // Implicit conversions between enum and ::Value
    constexpr PluralCategory(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * Construct from a string in the format
   * [specified in TR35](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules)
   *
   * See the [Rust documentation for `get_for_cldr_string`](https://docs.rs/icu/2.2.0/icu/plurals/enum.PluralCategory.html#method.get_for_cldr_string) for more information.
   *
   * See the [Rust documentation for `get_for_cldr_bytes`](https://docs.rs/icu/2.2.0/icu/plurals/enum.PluralCategory.html#method.get_for_cldr_bytes) for more information.
   */
  inline static std::optional<icu4x::PluralCategory> get_for_cldr_string(std::string_view s);

    inline icu4x::capi::PluralCategory AsFFI() const;
    inline static icu4x::PluralCategory FromFFI(icu4x::capi::PluralCategory c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_PluralCategory_D_HPP
