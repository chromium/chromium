#ifndef icu4x_DataProvider_D_HPP
#define icu4x_DataProvider_D_HPP

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
namespace capi { struct LocaleFallbacker; }
class LocaleFallbacker;
class DataError;
}


namespace icu4x {
namespace capi {
    struct DataProvider;
} // namespace capi
} // namespace

namespace icu4x {
class DataProvider {
public:

  inline static std::unique_ptr<icu4x::DataProvider> compiled();

  inline static diplomat::result<std::unique_ptr<icu4x::DataProvider>, icu4x::DataError> from_fs(std::string_view path);

  inline static diplomat::result<std::unique_ptr<icu4x::DataProvider>, icu4x::DataError> from_byte_slice(diplomat::span<const uint8_t> blob);

  inline static std::unique_ptr<icu4x::DataProvider> empty();

  inline diplomat::result<std::monostate, icu4x::DataError> fork_by_key(icu4x::DataProvider& other);

  inline diplomat::result<std::monostate, icu4x::DataError> fork_by_locale(icu4x::DataProvider& other);

  inline diplomat::result<std::monostate, icu4x::DataError> enable_locale_fallback_with(const icu4x::LocaleFallbacker& fallbacker);

  inline const icu4x::capi::DataProvider* AsFFI() const;
  inline icu4x::capi::DataProvider* AsFFI();
  inline static const icu4x::DataProvider* FromFFI(const icu4x::capi::DataProvider* ptr);
  inline static icu4x::DataProvider* FromFFI(icu4x::capi::DataProvider* ptr);
  inline static void operator delete(void* ptr);
private:
  DataProvider() = delete;
  DataProvider(const icu4x::DataProvider&) = delete;
  DataProvider(icu4x::DataProvider&&) noexcept = delete;
  DataProvider operator=(const icu4x::DataProvider&) = delete;
  DataProvider operator=(icu4x::DataProvider&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_DataProvider_D_HPP
