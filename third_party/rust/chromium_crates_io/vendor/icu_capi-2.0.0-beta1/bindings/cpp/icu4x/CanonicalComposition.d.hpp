#ifndef icu4x_CanonicalComposition_D_HPP
#define icu4x_CanonicalComposition_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CanonicalComposition; }
class CanonicalComposition;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
}


namespace icu4x {
namespace capi {
    struct CanonicalComposition;
} // namespace capi
} // namespace

namespace icu4x {
class CanonicalComposition {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::CanonicalComposition>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline char32_t compose(char32_t starter, char32_t second) const;

  inline const icu4x::capi::CanonicalComposition* AsFFI() const;
  inline icu4x::capi::CanonicalComposition* AsFFI();
  inline static const icu4x::CanonicalComposition* FromFFI(const icu4x::capi::CanonicalComposition* ptr);
  inline static icu4x::CanonicalComposition* FromFFI(icu4x::capi::CanonicalComposition* ptr);
  inline static void operator delete(void* ptr);
private:
  CanonicalComposition() = delete;
  CanonicalComposition(const icu4x::CanonicalComposition&) = delete;
  CanonicalComposition(icu4x::CanonicalComposition&&) noexcept = delete;
  CanonicalComposition operator=(const icu4x::CanonicalComposition&) = delete;
  CanonicalComposition operator=(icu4x::CanonicalComposition&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CanonicalComposition_D_HPP
