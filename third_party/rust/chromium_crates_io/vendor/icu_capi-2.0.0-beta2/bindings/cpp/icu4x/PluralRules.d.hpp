#ifndef icu4x_PluralRules_D_HPP
#define icu4x_PluralRules_D_HPP

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
namespace capi { struct Locale; }
class Locale;
namespace capi { struct PluralOperands; }
class PluralOperands;
namespace capi { struct PluralRules; }
class PluralRules;
struct PluralCategories;
class DataError;
class PluralCategory;
}


namespace icu4x {
namespace capi {
    struct PluralRules;
} // namespace capi
} // namespace

namespace icu4x {
class PluralRules {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_cardinal(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_cardinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_ordinal(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::PluralRules>, icu4x::DataError> create_ordinal_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline icu4x::PluralCategory category_for(const icu4x::PluralOperands& op) const;

  inline icu4x::PluralCategories categories() const;

  inline const icu4x::capi::PluralRules* AsFFI() const;
  inline icu4x::capi::PluralRules* AsFFI();
  inline static const icu4x::PluralRules* FromFFI(const icu4x::capi::PluralRules* ptr);
  inline static icu4x::PluralRules* FromFFI(icu4x::capi::PluralRules* ptr);
  inline static void operator delete(void* ptr);
private:
  PluralRules() = delete;
  PluralRules(const icu4x::PluralRules&) = delete;
  PluralRules(icu4x::PluralRules&&) noexcept = delete;
  PluralRules operator=(const icu4x::PluralRules&) = delete;
  PluralRules operator=(icu4x::PluralRules&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_PluralRules_D_HPP
