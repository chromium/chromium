#ifndef icu4x_LocaleFallbacker_D_HPP
#define icu4x_LocaleFallbacker_D_HPP

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
namespace capi { struct LocaleFallbackerWithConfig; }
class LocaleFallbackerWithConfig;
struct LocaleFallbackConfig;
class DataError;
}


namespace icu4x {
namespace capi {
    struct LocaleFallbacker;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleFallbacker {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleFallbacker>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::LocaleFallbacker> without_data();

  inline std::unique_ptr<icu4x::LocaleFallbackerWithConfig> for_config(icu4x::LocaleFallbackConfig config) const;

  inline const icu4x::capi::LocaleFallbacker* AsFFI() const;
  inline icu4x::capi::LocaleFallbacker* AsFFI();
  inline static const icu4x::LocaleFallbacker* FromFFI(const icu4x::capi::LocaleFallbacker* ptr);
  inline static icu4x::LocaleFallbacker* FromFFI(icu4x::capi::LocaleFallbacker* ptr);
  inline static void operator delete(void* ptr);
private:
  LocaleFallbacker() = delete;
  LocaleFallbacker(const icu4x::LocaleFallbacker&) = delete;
  LocaleFallbacker(icu4x::LocaleFallbacker&&) noexcept = delete;
  LocaleFallbacker operator=(const icu4x::LocaleFallbacker&) = delete;
  LocaleFallbacker operator=(icu4x::LocaleFallbacker&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LocaleFallbacker_D_HPP
