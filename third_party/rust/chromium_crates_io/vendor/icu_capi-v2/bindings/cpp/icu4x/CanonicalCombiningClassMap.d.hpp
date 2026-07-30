#ifndef ICU4X_CanonicalCombiningClassMap_D_HPP
#define ICU4X_CanonicalCombiningClassMap_D_HPP

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
namespace capi { struct CanonicalCombiningClassMap; }
class CanonicalCombiningClassMap;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CanonicalCombiningClassMap;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Lookup of the `Canonical_Combining_Class` Unicode property
 *
 * See the [Rust documentation for `CanonicalCombiningClassMap`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html) for more information.
 */
class CanonicalCombiningClassMap {
public:

  /**
   * Construct a new `CanonicalCombiningClassMap` instance for NFC using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::CanonicalCombiningClassMap> create();

  /**
   * Construct a new `CanonicalCombiningClassMap` instance for NFC using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/normalizer/properties/struct.CanonicalCombiningClassMapBorrowed.html#method.get) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/2.2.0/icu/properties/props/struct.CanonicalCombiningClass.html)
   */
  inline uint8_t operator[](char32_t ch) const;

    inline const icu4x::capi::CanonicalCombiningClassMap* AsFFI() const;
    inline icu4x::capi::CanonicalCombiningClassMap* AsFFI();
    inline static const icu4x::CanonicalCombiningClassMap* FromFFI(const icu4x::capi::CanonicalCombiningClassMap* ptr);
    inline static icu4x::CanonicalCombiningClassMap* FromFFI(icu4x::capi::CanonicalCombiningClassMap* ptr);
    inline static void operator delete(void* ptr);
private:
    CanonicalCombiningClassMap() = delete;
    CanonicalCombiningClassMap(const icu4x::CanonicalCombiningClassMap&) = delete;
    CanonicalCombiningClassMap(icu4x::CanonicalCombiningClassMap&&) noexcept = delete;
    CanonicalCombiningClassMap operator=(const icu4x::CanonicalCombiningClassMap&) = delete;
    CanonicalCombiningClassMap operator=(icu4x::CanonicalCombiningClassMap&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CanonicalCombiningClassMap_D_HPP
