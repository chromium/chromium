#ifndef icu4x_LocaleDisplayNamesFormatter_D_HPP
#define icu4x_LocaleDisplayNamesFormatter_D_HPP

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
namespace capi { struct LocaleDisplayNamesFormatter; }
class LocaleDisplayNamesFormatter;
struct DisplayNamesOptionsV1;
class DataError;
}


namespace icu4x {
namespace capi {
    struct LocaleDisplayNamesFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleDisplayNamesFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleDisplayNamesFormatter>, icu4x::DataError> create_v1(const icu4x::Locale& locale, icu4x::DisplayNamesOptionsV1 options);

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleDisplayNamesFormatter>, icu4x::DataError> create_v1_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DisplayNamesOptionsV1 options);

  inline std::string of(const icu4x::Locale& locale) const;

  inline const icu4x::capi::LocaleDisplayNamesFormatter* AsFFI() const;
  inline icu4x::capi::LocaleDisplayNamesFormatter* AsFFI();
  inline static const icu4x::LocaleDisplayNamesFormatter* FromFFI(const icu4x::capi::LocaleDisplayNamesFormatter* ptr);
  inline static icu4x::LocaleDisplayNamesFormatter* FromFFI(icu4x::capi::LocaleDisplayNamesFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  LocaleDisplayNamesFormatter() = delete;
  LocaleDisplayNamesFormatter(const icu4x::LocaleDisplayNamesFormatter&) = delete;
  LocaleDisplayNamesFormatter(icu4x::LocaleDisplayNamesFormatter&&) noexcept = delete;
  LocaleDisplayNamesFormatter operator=(const icu4x::LocaleDisplayNamesFormatter&) = delete;
  LocaleDisplayNamesFormatter operator=(icu4x::LocaleDisplayNamesFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LocaleDisplayNamesFormatter_D_HPP
