#ifndef icu4x_TitlecaseMapper_D_HPP
#define icu4x_TitlecaseMapper_D_HPP

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
namespace capi { struct TitlecaseMapper; }
class TitlecaseMapper;
struct TitlecaseOptionsV1;
class DataError;
}


namespace icu4x {
namespace capi {
    struct TitlecaseMapper;
} // namespace capi
} // namespace

namespace icu4x {
class TitlecaseMapper {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline diplomat::result<std::string, diplomat::Utf8Error> titlecase_segment_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) const;

  inline const icu4x::capi::TitlecaseMapper* AsFFI() const;
  inline icu4x::capi::TitlecaseMapper* AsFFI();
  inline static const icu4x::TitlecaseMapper* FromFFI(const icu4x::capi::TitlecaseMapper* ptr);
  inline static icu4x::TitlecaseMapper* FromFFI(icu4x::capi::TitlecaseMapper* ptr);
  inline static void operator delete(void* ptr);
private:
  TitlecaseMapper() = delete;
  TitlecaseMapper(const icu4x::TitlecaseMapper&) = delete;
  TitlecaseMapper(icu4x::TitlecaseMapper&&) noexcept = delete;
  TitlecaseMapper operator=(const icu4x::TitlecaseMapper&) = delete;
  TitlecaseMapper operator=(icu4x::TitlecaseMapper&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TitlecaseMapper_D_HPP
