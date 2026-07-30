#ifndef ICU4X_CaseMapLocales_D_HPP
#define ICU4X_CaseMapLocales_D_HPP

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
namespace capi { struct Locale; }
class Locale;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CaseMapLocales;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A type that provides access to preconstructred Locales, to avoid
 * needing to parse user locales when the code knows what locale it wants.
 *
 * In most cases, you should be taking a locale from the user, but in some
 * limited cases you may be parsing from a limited set of locales and not
 * wish to pull in full-fledged parsing code.
 *
 * This type is for locales that have special meaning to `CaseMapper`,
 * since it only cares about a small set of locales and locale parsing takes
 * up a relatively large amount of binary size in the context of casemapping.
 */
class CaseMapLocales {
public:

  /**
   * Returns a borrowed "az" {@link Locale}, without allocating.
   */
  inline static const icu4x::Locale& az();

  /**
   * Returns a borrowed "el" {@link Locale}, without allocating.
   */
  inline static const icu4x::Locale& el();

  /**
   * Returns a borrowed "hy" {@link Locale}, without allocating.
   */
  inline static const icu4x::Locale& hy();

  /**
   * Returns a borrowed "lt" {@link Locale}, without allocating.
   */
  inline static const icu4x::Locale& lt();

  /**
   * Returns a borrowed "nl" {@link Locale}, without allocating.
   */
  inline static const icu4x::Locale& nl();

  /**
   * Returns a borrowed "tr" {@link Locale}, without allocating.
   */
  inline static const icu4x::Locale& tr();

    inline const icu4x::capi::CaseMapLocales* AsFFI() const;
    inline icu4x::capi::CaseMapLocales* AsFFI();
    inline static const icu4x::CaseMapLocales* FromFFI(const icu4x::capi::CaseMapLocales* ptr);
    inline static icu4x::CaseMapLocales* FromFFI(icu4x::capi::CaseMapLocales* ptr);
    inline static void operator delete(void* ptr);
private:
    CaseMapLocales() = delete;
    CaseMapLocales(const icu4x::CaseMapLocales&) = delete;
    CaseMapLocales(icu4x::CaseMapLocales&&) noexcept = delete;
    CaseMapLocales operator=(const icu4x::CaseMapLocales&) = delete;
    CaseMapLocales operator=(icu4x::CaseMapLocales&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_CaseMapLocales_D_HPP
