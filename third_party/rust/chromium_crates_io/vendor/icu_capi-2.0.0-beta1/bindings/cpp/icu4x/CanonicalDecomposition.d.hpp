#ifndef icu4x_CanonicalDecomposition_D_HPP
#define icu4x_CanonicalDecomposition_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CanonicalDecomposition; }
class CanonicalDecomposition;
namespace capi { struct DataProvider; }
class DataProvider;
struct Decomposed;
class DataError;
}


namespace icu4x {
namespace capi {
    struct CanonicalDecomposition;
} // namespace capi
} // namespace

namespace icu4x {
class CanonicalDecomposition {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::CanonicalDecomposition>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline icu4x::Decomposed decompose(char32_t c) const;

  inline const icu4x::capi::CanonicalDecomposition* AsFFI() const;
  inline icu4x::capi::CanonicalDecomposition* AsFFI();
  inline static const icu4x::CanonicalDecomposition* FromFFI(const icu4x::capi::CanonicalDecomposition* ptr);
  inline static icu4x::CanonicalDecomposition* FromFFI(icu4x::capi::CanonicalDecomposition* ptr);
  inline static void operator delete(void* ptr);
private:
  CanonicalDecomposition() = delete;
  CanonicalDecomposition(const icu4x::CanonicalDecomposition&) = delete;
  CanonicalDecomposition(icu4x::CanonicalDecomposition&&) noexcept = delete;
  CanonicalDecomposition operator=(const icu4x::CanonicalDecomposition&) = delete;
  CanonicalDecomposition operator=(icu4x::CanonicalDecomposition&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CanonicalDecomposition_D_HPP
