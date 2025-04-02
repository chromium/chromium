#ifndef icu4x_LocaleExpander_D_HPP
#define icu4x_LocaleExpander_D_HPP

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
namespace capi { struct LocaleExpander; }
class LocaleExpander;
class DataError;
class TransformResult;
}


namespace icu4x {
namespace capi {
    struct LocaleExpander;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleExpander {
public:

  inline static std::unique_ptr<icu4x::LocaleExpander> create_common();

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleExpander>, icu4x::DataError> create_common_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::LocaleExpander> create_extended();

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleExpander>, icu4x::DataError> create_extended_with_provider(const icu4x::DataProvider& provider);

  inline icu4x::TransformResult maximize(icu4x::Locale& locale) const;

  inline icu4x::TransformResult minimize(icu4x::Locale& locale) const;

  inline icu4x::TransformResult minimize_favor_script(icu4x::Locale& locale) const;

  inline const icu4x::capi::LocaleExpander* AsFFI() const;
  inline icu4x::capi::LocaleExpander* AsFFI();
  inline static const icu4x::LocaleExpander* FromFFI(const icu4x::capi::LocaleExpander* ptr);
  inline static icu4x::LocaleExpander* FromFFI(icu4x::capi::LocaleExpander* ptr);
  inline static void operator delete(void* ptr);
private:
  LocaleExpander() = delete;
  LocaleExpander(const icu4x::LocaleExpander&) = delete;
  LocaleExpander(icu4x::LocaleExpander&&) noexcept = delete;
  LocaleExpander operator=(const icu4x::LocaleExpander&) = delete;
  LocaleExpander operator=(icu4x::LocaleExpander&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LocaleExpander_D_HPP
