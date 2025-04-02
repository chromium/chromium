#ifndef icu4x_GeneralCategoryNameToGroupMapper_D_HPP
#define icu4x_GeneralCategoryNameToGroupMapper_D_HPP

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
namespace capi { struct GeneralCategoryNameToGroupMapper; }
class GeneralCategoryNameToGroupMapper;
struct GeneralCategoryGroup;
class DataError;
}


namespace icu4x {
namespace capi {
    struct GeneralCategoryNameToGroupMapper;
} // namespace capi
} // namespace

namespace icu4x {
class GeneralCategoryNameToGroupMapper {
public:

  inline icu4x::GeneralCategoryGroup get_strict(std::string_view name) const;

  inline icu4x::GeneralCategoryGroup get_loose(std::string_view name) const;

  inline static std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper> create();

  inline static diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline const icu4x::capi::GeneralCategoryNameToGroupMapper* AsFFI() const;
  inline icu4x::capi::GeneralCategoryNameToGroupMapper* AsFFI();
  inline static const icu4x::GeneralCategoryNameToGroupMapper* FromFFI(const icu4x::capi::GeneralCategoryNameToGroupMapper* ptr);
  inline static icu4x::GeneralCategoryNameToGroupMapper* FromFFI(icu4x::capi::GeneralCategoryNameToGroupMapper* ptr);
  inline static void operator delete(void* ptr);
private:
  GeneralCategoryNameToGroupMapper() = delete;
  GeneralCategoryNameToGroupMapper(const icu4x::GeneralCategoryNameToGroupMapper&) = delete;
  GeneralCategoryNameToGroupMapper(icu4x::GeneralCategoryNameToGroupMapper&&) noexcept = delete;
  GeneralCategoryNameToGroupMapper operator=(const icu4x::GeneralCategoryNameToGroupMapper&) = delete;
  GeneralCategoryNameToGroupMapper operator=(icu4x::GeneralCategoryNameToGroupMapper&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_GeneralCategoryNameToGroupMapper_D_HPP
