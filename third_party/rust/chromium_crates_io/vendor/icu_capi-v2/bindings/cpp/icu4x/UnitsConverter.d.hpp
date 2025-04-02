#ifndef icu4x_UnitsConverter_D_HPP
#define icu4x_UnitsConverter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct UnitsConverter; }
class UnitsConverter;
}


namespace icu4x {
namespace capi {
    struct UnitsConverter;
} // namespace capi
} // namespace

namespace icu4x {
class UnitsConverter {
public:

  inline double convert(double value) const;

  inline std::unique_ptr<icu4x::UnitsConverter> clone() const;

  inline const icu4x::capi::UnitsConverter* AsFFI() const;
  inline icu4x::capi::UnitsConverter* AsFFI();
  inline static const icu4x::UnitsConverter* FromFFI(const icu4x::capi::UnitsConverter* ptr);
  inline static icu4x::UnitsConverter* FromFFI(icu4x::capi::UnitsConverter* ptr);
  inline static void operator delete(void* ptr);
private:
  UnitsConverter() = delete;
  UnitsConverter(const icu4x::UnitsConverter&) = delete;
  UnitsConverter(icu4x::UnitsConverter&&) noexcept = delete;
  UnitsConverter operator=(const icu4x::UnitsConverter&) = delete;
  UnitsConverter operator=(icu4x::UnitsConverter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_UnitsConverter_D_HPP
