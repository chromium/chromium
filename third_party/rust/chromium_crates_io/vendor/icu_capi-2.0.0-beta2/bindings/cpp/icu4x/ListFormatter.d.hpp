#ifndef icu4x_ListFormatter_D_HPP
#define icu4x_ListFormatter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct ListFormatter; }
class ListFormatter;
namespace capi { struct Locale; }
class Locale;
class DataError;
class ListLength;
}


namespace icu4x {
namespace capi {
    struct ListFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class ListFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_and_with_length(const icu4x::Locale& locale, icu4x::ListLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_and_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_or_with_length(const icu4x::Locale& locale, icu4x::ListLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_or_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_unit_with_length(const icu4x::Locale& locale, icu4x::ListLength length);

  inline static diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> create_unit_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length);

  inline std::string format(diplomat::span<const std::string_view> list) const;

  inline std::string format16(diplomat::span<const std::u16string_view> list) const;

  inline const icu4x::capi::ListFormatter* AsFFI() const;
  inline icu4x::capi::ListFormatter* AsFFI();
  inline static const icu4x::ListFormatter* FromFFI(const icu4x::capi::ListFormatter* ptr);
  inline static icu4x::ListFormatter* FromFFI(icu4x::capi::ListFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  ListFormatter() = delete;
  ListFormatter(const icu4x::ListFormatter&) = delete;
  ListFormatter(icu4x::ListFormatter&&) noexcept = delete;
  ListFormatter operator=(const icu4x::ListFormatter&) = delete;
  ListFormatter operator=(icu4x::ListFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_ListFormatter_D_HPP
