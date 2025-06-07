#ifndef temporal_rs_TimeZone_D_HPP
#define temporal_rs_TimeZone_D_HPP

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
namespace capi { struct TimeZone; }
class TimeZone;
struct TemporalError;
}


namespace temporal_rs {
namespace capi {
    struct TimeZone;
} // namespace capi
} // namespace

namespace temporal_rs {
class TimeZone {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> try_from_identifier_str(std::string_view ident);

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> try_from_str(std::string_view ident);

  inline const temporal_rs::capi::TimeZone* AsFFI() const;
  inline temporal_rs::capi::TimeZone* AsFFI();
  inline static const temporal_rs::TimeZone* FromFFI(const temporal_rs::capi::TimeZone* ptr);
  inline static temporal_rs::TimeZone* FromFFI(temporal_rs::capi::TimeZone* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZone() = delete;
  TimeZone(const temporal_rs::TimeZone&) = delete;
  TimeZone(temporal_rs::TimeZone&&) noexcept = delete;
  TimeZone operator=(const temporal_rs::TimeZone&) = delete;
  TimeZone operator=(temporal_rs::TimeZone&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_TimeZone_D_HPP
