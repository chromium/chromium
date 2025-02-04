#ifndef icu4x_GeneralCategoryNameToMaskMapper_D_HPP
#define icu4x_GeneralCategoryNameToMaskMapper_D_HPP

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
namespace capi { struct GeneralCategoryNameToMaskMapper; }
class GeneralCategoryNameToMaskMapper;
class DataError;
}


namespace icu4x {
namespace capi {
    struct GeneralCategoryNameToMaskMapper;
} // namespace capi
} // namespace

namespace icu4x {
class GeneralCategoryNameToMaskMapper {
public:

  inline uint32_t get_strict(std::string_view name) const;

  inline uint32_t get_loose(std::string_view name) const;

  inline static diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToMaskMapper>, icu4x::DataError> load(const icu4x::DataProvider& provider);

  inline const icu4x::capi::GeneralCategoryNameToMaskMapper* AsFFI() const;
  inline icu4x::capi::GeneralCategoryNameToMaskMapper* AsFFI();
  inline static const icu4x::GeneralCategoryNameToMaskMapper* FromFFI(const icu4x::capi::GeneralCategoryNameToMaskMapper* ptr);
  inline static icu4x::GeneralCategoryNameToMaskMapper* FromFFI(icu4x::capi::GeneralCategoryNameToMaskMapper* ptr);
  inline static void operator delete(void* ptr);
private:
  GeneralCategoryNameToMaskMapper() = delete;
  GeneralCategoryNameToMaskMapper(const icu4x::GeneralCategoryNameToMaskMapper&) = delete;
  GeneralCategoryNameToMaskMapper(icu4x::GeneralCategoryNameToMaskMapper&&) noexcept = delete;
  GeneralCategoryNameToMaskMapper operator=(const icu4x::GeneralCategoryNameToMaskMapper&) = delete;
  GeneralCategoryNameToMaskMapper operator=(icu4x::GeneralCategoryNameToMaskMapper&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GeneralCategoryNameToMaskMapper_D_HPP
