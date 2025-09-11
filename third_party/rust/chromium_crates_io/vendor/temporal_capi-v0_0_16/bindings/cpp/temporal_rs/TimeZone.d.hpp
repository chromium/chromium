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
namespace capi { struct Provider; }
class Provider;
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

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> try_from_identifier_str_with_provider(std::string_view ident, const temporal_rs::Provider& p);

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> try_from_offset_str(std::string_view ident);

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> try_from_str(std::string_view ident);

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> try_from_str_with_provider(std::string_view ident, const temporal_rs::Provider& p);

  inline std::string identifier() const;
  template<typename W>
  inline void identifier_write(W& writeable_output) const;

  inline diplomat::result<std::string, temporal_rs::TemporalError> identifier_with_provider(const temporal_rs::Provider& p) const;
  template<typename W>
  inline diplomat::result<std::monostate, temporal_rs::TemporalError> identifier_with_provider_write(const temporal_rs::Provider& p, W& writeable_output) const;

  inline static std::unique_ptr<temporal_rs::TimeZone> utc();

  /**
   * Create a TimeZone that represents +00:00
   *
   * This is the only way to infallibly make a TimeZone without compiled_data,
   * and can be used as a fallback.
   */
  inline static std::unique_ptr<temporal_rs::TimeZone> zero();

  inline static diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> utc_with_provider(const temporal_rs::Provider& p);

  inline std::unique_ptr<temporal_rs::TimeZone> clone() const;

  /**
   * Get the primary time zone identifier corresponding to this time zone
   */
  inline diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> primary_identifier() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::TimeZone>, temporal_rs::TemporalError> primary_identifier_with_provider(const temporal_rs::Provider& p) const;

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
