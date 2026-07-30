#ifndef ICU4X_GeneralCategoryNameToGroupMapper_D_HPP
#define ICU4X_GeneralCategoryNameToGroupMapper_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct GeneralCategoryNameToGroupMapper; }
class GeneralCategoryNameToGroupMapper;
struct GeneralCategoryGroup;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct GeneralCategoryNameToGroupMapper;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A type capable of looking up General Category Group values from a string name.
 *
 * See the [Rust documentation for `PropertyParser`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyParser.html) for more information.
 *
 * See the [Rust documentation for `GeneralCategory`](https://docs.rs/icu/2.2.0/icu/properties/props/enum.GeneralCategory.html) for more information.
 */
class GeneralCategoryNameToGroupMapper {
public:

  /**
   * Get the mask value matching the given name, using strict matching
   *
   * Returns 0 if the name is unknown for this property
   *
   * See the [Rust documentation for `get_strict`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyParserBorrowed.html#method.get_strict) for more information.
   */
  inline icu4x::GeneralCategoryGroup get_strict(std::string_view name) const;

  /**
   * Get the mask value matching the given name, using loose matching
   *
   * Returns 0 if the name is unknown for this property
   *
   * See the [Rust documentation for `get_loose`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyParserBorrowed.html#method.get_loose) for more information.
   */
  inline icu4x::GeneralCategoryGroup get_loose(std::string_view name) const;

  /**
   * Create a name-to-mask mapper for the `General_Category` property, using compiled data.
   *
   * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html) for more information.
   */
  inline static std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper> create();

  /**
   * Create a name-to-mask mapper for the `General_Category` property, using a particular data source.
   *
   * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.GeneralCategoryGroup.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

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
#endif // ICU4X_GeneralCategoryNameToGroupMapper_D_HPP
