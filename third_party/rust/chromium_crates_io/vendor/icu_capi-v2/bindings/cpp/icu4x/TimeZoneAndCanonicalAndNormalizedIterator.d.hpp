#ifndef icu4x_TimeZoneAndCanonicalAndNormalizedIterator_D_HPP
#define icu4x_TimeZoneAndCanonicalAndNormalizedIterator_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
struct TimeZoneAndCanonicalAndNormalized;
}


namespace icu4x {
namespace capi {
    struct TimeZoneAndCanonicalAndNormalizedIterator;
} // namespace capi
} // namespace

namespace icu4x {
class TimeZoneAndCanonicalAndNormalizedIterator {
public:

  inline std::optional<icu4x::TimeZoneAndCanonicalAndNormalized> next();

  inline const icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* AsFFI() const;
  inline icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* AsFFI();
  inline static const icu4x::TimeZoneAndCanonicalAndNormalizedIterator* FromFFI(const icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* ptr);
  inline static icu4x::TimeZoneAndCanonicalAndNormalizedIterator* FromFFI(icu4x::capi::TimeZoneAndCanonicalAndNormalizedIterator* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZoneAndCanonicalAndNormalizedIterator() = delete;
  TimeZoneAndCanonicalAndNormalizedIterator(const icu4x::TimeZoneAndCanonicalAndNormalizedIterator&) = delete;
  TimeZoneAndCanonicalAndNormalizedIterator(icu4x::TimeZoneAndCanonicalAndNormalizedIterator&&) noexcept = delete;
  TimeZoneAndCanonicalAndNormalizedIterator operator=(const icu4x::TimeZoneAndCanonicalAndNormalizedIterator&) = delete;
  TimeZoneAndCanonicalAndNormalizedIterator operator=(icu4x::TimeZoneAndCanonicalAndNormalizedIterator&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeZoneAndCanonicalAndNormalizedIterator_D_HPP
