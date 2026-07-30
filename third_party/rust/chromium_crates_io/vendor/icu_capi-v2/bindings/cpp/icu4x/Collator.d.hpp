#ifndef ICU4X_Collator_D_HPP
#define ICU4X_Collator_D_HPP

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
namespace capi { struct Collator; }
class Collator;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Locale; }
class Locale;
struct CollatorOptionsV1;
struct CollatorResolvedOptionsV1;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct Collator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Collator`](https://docs.rs/icu/2.2.0/icu/collator/struct.Collator.html) for more information.
 */
class Collator {
public:

  /**
   * Construct a new Collator instance using compiled data.
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/collator/struct.Collator.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Collator>, icu4x::DataError> create_v1(const icu4x::Locale& locale, icu4x::CollatorOptionsV1 options);

  /**
   * Construct a new Collator instance using a particular data source.
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/collator/struct.Collator.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Collator>, icu4x::DataError> create_v1_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::CollatorOptionsV1 options);

  /**
   * Compare two strings.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `compare_utf8`](https://docs.rs/icu/2.2.0/icu/collator/struct.CollatorBorrowed.html#method.compare_utf8) for more information.
   */
  inline int8_t compare(std::string_view left, std::string_view right) const;

  /**
   * Compare two strings.
   *
   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   *
   * See the [Rust documentation for `compare_utf16`](https://docs.rs/icu/2.2.0/icu/collator/struct.CollatorBorrowed.html#method.compare_utf16) for more information.
   */
  inline int8_t compare16(std::u16string_view left, std::u16string_view right) const;

  /**
   * The resolved options showing how the default options, the requested options,
   * and the options from locale data were combined. None of the struct fields
   * will have `Auto` as the value.
   *
   * See the [Rust documentation for `resolved_options`](https://docs.rs/icu/2.2.0/icu/collator/struct.CollatorBorrowed.html#method.resolved_options) for more information.
   */
  inline icu4x::CollatorResolvedOptionsV1 resolved_options_v1() const;

    inline const icu4x::capi::Collator* AsFFI() const;
    inline icu4x::capi::Collator* AsFFI();
    inline static const icu4x::Collator* FromFFI(const icu4x::capi::Collator* ptr);
    inline static icu4x::Collator* FromFFI(icu4x::capi::Collator* ptr);
    inline static void operator delete(void* ptr);
private:
    Collator() = delete;
    Collator(const icu4x::Collator&) = delete;
    Collator(icu4x::Collator&&) noexcept = delete;
    Collator operator=(const icu4x::Collator&) = delete;
    Collator operator=(icu4x::Collator&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_Collator_D_HPP
