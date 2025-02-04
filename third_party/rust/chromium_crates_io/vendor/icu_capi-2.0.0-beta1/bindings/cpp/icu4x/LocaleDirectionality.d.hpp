#ifndef icu4x_LocaleDirectionality_D_HPP
#define icu4x_LocaleDirectionality_D_HPP

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
namespace capi { struct LocaleDirectionality; }
class LocaleDirectionality;
namespace capi { struct LocaleExpander; }
class LocaleExpander;
class DataError;
class LocaleDirection;
}


namespace icu4x {
namespace capi {
    struct LocaleDirectionality;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleDirectionality {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleDirectionality>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleDirectionality>, icu4x::DataError> create_with_expander(const icu4x::DataProvider& provider, const icu4x::LocaleExpander& expander);

  inline icu4x::LocaleDirection get(const icu4x::Locale& locale) const;

  inline bool is_left_to_right(const icu4x::Locale& locale) const;

  inline bool is_right_to_left(const icu4x::Locale& locale) const;

  inline const icu4x::capi::LocaleDirectionality* AsFFI() const;
  inline icu4x::capi::LocaleDirectionality* AsFFI();
  inline static const icu4x::LocaleDirectionality* FromFFI(const icu4x::capi::LocaleDirectionality* ptr);
  inline static icu4x::LocaleDirectionality* FromFFI(icu4x::capi::LocaleDirectionality* ptr);
  inline static void operator delete(void* ptr);
private:
  LocaleDirectionality() = delete;
  LocaleDirectionality(const icu4x::LocaleDirectionality&) = delete;
  LocaleDirectionality(icu4x::LocaleDirectionality&&) noexcept = delete;
  LocaleDirectionality operator=(const icu4x::LocaleDirectionality&) = delete;
  LocaleDirectionality operator=(icu4x::LocaleDirectionality&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LocaleDirectionality_D_HPP
