#ifndef icu4x_TimeZoneIdMapperWithFastCanonicalization_D_HPP
#define icu4x_TimeZoneIdMapperWithFastCanonicalization_D_HPP

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
namespace capi { struct TimeZoneIdMapperWithFastCanonicalization; }
class TimeZoneIdMapperWithFastCanonicalization;
class DataError;
}


namespace icu4x {
namespace capi {
    struct TimeZoneIdMapperWithFastCanonicalization;
} // namespace capi
} // namespace

namespace icu4x {
class TimeZoneIdMapperWithFastCanonicalization {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapperWithFastCanonicalization>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline diplomat::result<std::optional<std::string>, diplomat::Utf8Error> canonicalize_iana(std::string_view value) const;

  inline std::optional<std::string> canonical_iana_from_bcp47(std::string_view value) const;

  inline const icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* AsFFI() const;
  inline icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* AsFFI();
  inline static const icu4x::TimeZoneIdMapperWithFastCanonicalization* FromFFI(const icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* ptr);
  inline static icu4x::TimeZoneIdMapperWithFastCanonicalization* FromFFI(icu4x::capi::TimeZoneIdMapperWithFastCanonicalization* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZoneIdMapperWithFastCanonicalization() = delete;
  TimeZoneIdMapperWithFastCanonicalization(const icu4x::TimeZoneIdMapperWithFastCanonicalization&) = delete;
  TimeZoneIdMapperWithFastCanonicalization(icu4x::TimeZoneIdMapperWithFastCanonicalization&&) noexcept = delete;
  TimeZoneIdMapperWithFastCanonicalization operator=(const icu4x::TimeZoneIdMapperWithFastCanonicalization&) = delete;
  TimeZoneIdMapperWithFastCanonicalization operator=(icu4x::TimeZoneIdMapperWithFastCanonicalization&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeZoneIdMapperWithFastCanonicalization_D_HPP
