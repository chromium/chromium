#ifndef icu4x_LocaleCanonicalizer_D_HPP
#define icu4x_LocaleCanonicalizer_D_HPP

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
namespace capi { struct LocaleCanonicalizer; }
class LocaleCanonicalizer;
class DataError;
class TransformResult;
}


namespace icu4x {
namespace capi {
    struct LocaleCanonicalizer;
} // namespace capi
} // namespace

namespace icu4x {
class LocaleCanonicalizer {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::LocaleCanonicalizer>, icu4x::DataError> create_extended(const icu4x::DataProvider& provider);

  inline icu4x::TransformResult canonicalize(icu4x::Locale& locale) const;

  inline const icu4x::capi::LocaleCanonicalizer* AsFFI() const;
  inline icu4x::capi::LocaleCanonicalizer* AsFFI();
  inline static const icu4x::LocaleCanonicalizer* FromFFI(const icu4x::capi::LocaleCanonicalizer* ptr);
  inline static icu4x::LocaleCanonicalizer* FromFFI(icu4x::capi::LocaleCanonicalizer* ptr);
  inline static void operator delete(void* ptr);
private:
  LocaleCanonicalizer() = delete;
  LocaleCanonicalizer(const icu4x::LocaleCanonicalizer&) = delete;
  LocaleCanonicalizer(icu4x::LocaleCanonicalizer&&) noexcept = delete;
  LocaleCanonicalizer operator=(const icu4x::LocaleCanonicalizer&) = delete;
  LocaleCanonicalizer operator=(icu4x::LocaleCanonicalizer&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_LocaleCanonicalizer_D_HPP
