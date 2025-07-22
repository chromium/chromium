#ifndef temporal_rs_OwnedPartialZonedDateTime_D_HPP
#define temporal_rs_OwnedPartialZonedDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"

namespace temporal_rs {
namespace capi { struct OwnedPartialZonedDateTime; }
class OwnedPartialZonedDateTime;
struct TemporalError;
}


namespace temporal_rs {
namespace capi {
    struct OwnedPartialZonedDateTime;
} // namespace capi
} // namespace

namespace temporal_rs {
class OwnedPartialZonedDateTime {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline const temporal_rs::capi::OwnedPartialZonedDateTime* AsFFI() const;
  inline temporal_rs::capi::OwnedPartialZonedDateTime* AsFFI();
  inline static const temporal_rs::OwnedPartialZonedDateTime* FromFFI(const temporal_rs::capi::OwnedPartialZonedDateTime* ptr);
  inline static temporal_rs::OwnedPartialZonedDateTime* FromFFI(temporal_rs::capi::OwnedPartialZonedDateTime* ptr);
  inline static void operator delete(void* ptr);
private:
  OwnedPartialZonedDateTime() = delete;
  OwnedPartialZonedDateTime(const temporal_rs::OwnedPartialZonedDateTime&) = delete;
  OwnedPartialZonedDateTime(temporal_rs::OwnedPartialZonedDateTime&&) noexcept = delete;
  OwnedPartialZonedDateTime operator=(const temporal_rs::OwnedPartialZonedDateTime&) = delete;
  OwnedPartialZonedDateTime operator=(temporal_rs::OwnedPartialZonedDateTime&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_OwnedPartialZonedDateTime_D_HPP
