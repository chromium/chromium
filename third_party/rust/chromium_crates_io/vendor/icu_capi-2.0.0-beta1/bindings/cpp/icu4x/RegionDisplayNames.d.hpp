#ifndef icu4x_RegionDisplayNames_D_HPP
#define icu4x_RegionDisplayNames_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct RegionDisplayNames; }
class RegionDisplayNames;
class DataError;
class LocaleParseError;
}


namespace icu4x {
namespace capi {
    struct RegionDisplayNames;
} // namespace capi
} // namespace

namespace icu4x {
class RegionDisplayNames {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::RegionDisplayNames>, icu4x::DataError> create(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline diplomat::result<std::string, icu4x::LocaleParseError> of(std::string_view region) const;

  inline const icu4x::capi::RegionDisplayNames* AsFFI() const;
  inline icu4x::capi::RegionDisplayNames* AsFFI();
  inline static const icu4x::RegionDisplayNames* FromFFI(const icu4x::capi::RegionDisplayNames* ptr);
  inline static icu4x::RegionDisplayNames* FromFFI(icu4x::capi::RegionDisplayNames* ptr);
  inline static void operator delete(void* ptr);
private:
  RegionDisplayNames() = delete;
  RegionDisplayNames(const icu4x::RegionDisplayNames&) = delete;
  RegionDisplayNames(icu4x::RegionDisplayNames&&) noexcept = delete;
  RegionDisplayNames operator=(const icu4x::RegionDisplayNames&) = delete;
  RegionDisplayNames operator=(icu4x::RegionDisplayNames&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_RegionDisplayNames_D_HPP
