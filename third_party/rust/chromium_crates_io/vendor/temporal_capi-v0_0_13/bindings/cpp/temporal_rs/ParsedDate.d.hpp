#ifndef temporal_rs_ParsedDate_D_HPP
#define temporal_rs_ParsedDate_D_HPP

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
namespace capi { struct ParsedDate; }
class ParsedDate;
struct TemporalError;
}


namespace temporal_rs {
namespace capi {
    struct ParsedDate;
} // namespace capi
} // namespace

namespace temporal_rs {
class ParsedDate {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> year_month_from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> year_month_from_utf16(std::u16string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> month_day_from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> month_day_from_utf16(std::u16string_view s);

  inline const temporal_rs::capi::ParsedDate* AsFFI() const;
  inline temporal_rs::capi::ParsedDate* AsFFI();
  inline static const temporal_rs::ParsedDate* FromFFI(const temporal_rs::capi::ParsedDate* ptr);
  inline static temporal_rs::ParsedDate* FromFFI(temporal_rs::capi::ParsedDate* ptr);
  inline static void operator delete(void* ptr);
private:
  ParsedDate() = delete;
  ParsedDate(const temporal_rs::ParsedDate&) = delete;
  ParsedDate(temporal_rs::ParsedDate&&) noexcept = delete;
  ParsedDate operator=(const temporal_rs::ParsedDate&) = delete;
  ParsedDate operator=(temporal_rs::ParsedDate&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_ParsedDate_D_HPP
