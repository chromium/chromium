#ifndef icu4x_TimeZoneIdMapper_D_HPP
#define icu4x_TimeZoneIdMapper_D_HPP

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
namespace capi { struct TimeZoneIdMapper; }
class TimeZoneIdMapper;
class DataError;
}


namespace icu4x {
namespace capi {
    struct TimeZoneIdMapper;
} // namespace capi
} // namespace

namespace icu4x {
class TimeZoneIdMapper {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::TimeZoneIdMapper>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline std::string iana_to_bcp47(std::string_view value) const;

  inline diplomat::result<std::optional<std::string>, diplomat::Utf8Error> normalize_iana(std::string_view value) const;

  inline diplomat::result<std::optional<std::string>, diplomat::Utf8Error> canonicalize_iana(std::string_view value) const;

  inline std::optional<std::string> find_canonical_iana_from_bcp47(std::string_view value) const;

  inline const icu4x::capi::TimeZoneIdMapper* AsFFI() const;
  inline icu4x::capi::TimeZoneIdMapper* AsFFI();
  inline static const icu4x::TimeZoneIdMapper* FromFFI(const icu4x::capi::TimeZoneIdMapper* ptr);
  inline static icu4x::TimeZoneIdMapper* FromFFI(icu4x::capi::TimeZoneIdMapper* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZoneIdMapper() = delete;
  TimeZoneIdMapper(const icu4x::TimeZoneIdMapper&) = delete;
  TimeZoneIdMapper(icu4x::TimeZoneIdMapper&&) noexcept = delete;
  TimeZoneIdMapper operator=(const icu4x::TimeZoneIdMapper&) = delete;
  TimeZoneIdMapper operator=(icu4x::TimeZoneIdMapper&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeZoneIdMapper_D_HPP
