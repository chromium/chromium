#ifndef icu4x_Locale_D_HPP
#define icu4x_Locale_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Locale; }
class Locale;
class LocaleParseError;
}


namespace icu4x {
namespace capi {
    struct Locale;
} // namespace capi
} // namespace

namespace icu4x {
class Locale {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError> from_string(std::string_view name);

  inline static std::unique_ptr<icu4x::Locale> und();

  inline std::unique_ptr<icu4x::Locale> clone() const;

  inline std::string basename() const;

  inline std::optional<std::string> get_unicode_extension(std::string_view s) const;

  inline std::string language() const;

  inline diplomat::result<std::monostate, icu4x::LocaleParseError> set_language(std::string_view s);

  inline std::optional<std::string> region() const;

  inline diplomat::result<std::monostate, icu4x::LocaleParseError> set_region(std::string_view s);

  inline std::optional<std::string> script() const;

  inline diplomat::result<std::monostate, icu4x::LocaleParseError> set_script(std::string_view s);

  inline static diplomat::result<std::string, icu4x::LocaleParseError> normalize(std::string_view s);

  inline std::string to_string() const;

  inline bool normalizing_eq(std::string_view other) const;

  inline int8_t compare_to_string(std::string_view other) const;

  inline int8_t compare_to(const icu4x::Locale& other) const;

  inline const icu4x::capi::Locale* AsFFI() const;
  inline icu4x::capi::Locale* AsFFI();
  inline static const icu4x::Locale* FromFFI(const icu4x::capi::Locale* ptr);
  inline static icu4x::Locale* FromFFI(icu4x::capi::Locale* ptr);
  inline static void operator delete(void* ptr);
private:
  Locale() = delete;
  Locale(const icu4x::Locale&) = delete;
  Locale(icu4x::Locale&&) noexcept = delete;
  Locale operator=(const icu4x::Locale&) = delete;
  Locale operator=(icu4x::Locale&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_Locale_D_HPP
