#ifndef icu4x_CanonicalCombiningClassMap_D_HPP
#define icu4x_CanonicalCombiningClassMap_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CanonicalCombiningClassMap; }
class CanonicalCombiningClassMap;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
}


namespace icu4x {
namespace capi {
    struct CanonicalCombiningClassMap;
} // namespace capi
} // namespace

namespace icu4x {
class CanonicalCombiningClassMap {
public:

  inline static std::unique_ptr<icu4x::CanonicalCombiningClassMap> create();

  inline static diplomat::result<std::unique_ptr<icu4x::CanonicalCombiningClassMap>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline uint8_t get(char32_t ch) const;

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
#endif // icu4x_CanonicalCombiningClassMap_D_HPP
