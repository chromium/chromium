#ifndef ICU4X_Locale_D_HPP
#define ICU4X_Locale_D_HPP

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
class LocaleParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct Locale;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Locale, capable of representing strings like `"en-US"`.
 *
 * See the [Rust documentation for `Locale`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html) for more information.
 */
class Locale {
public:

  /**
   * Construct an {@link Locale} from an locale identifier.
   *
   * This will run the complete locale parsing algorithm. If code size and
   * performance are critical and the locale is of a known shape (such as
   * `aa-BB`) use `create_und`, `set_language`, `set_script`, and `set_region`.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError> from_string(std::string_view name);

  /**
   * Construct a unknown {@link Locale} "und".
   *
   * See the [Rust documentation for `UNKNOWN`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#associatedconstant.UNKNOWN) for more information.
   */
  inline static std::unique_ptr<icu4x::Locale> unknown();

  /**
   * Returns a borrowed unknown ("und") {@link Locale}, without allocating.
   *
   * See the [Rust documentation for `UNKNOWN`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#associatedconstant.UNKNOWN) for more information.
   */
  inline static const icu4x::Locale& unknown_ref();

  /**
   * Clones the {@link Locale}.
   *
   * See the [Rust documentation for `Locale`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html) for more information.
   */
  inline std::unique_ptr<icu4x::Locale> clone() const;

  /**
   * Returns a string representation of the `LanguageIdentifier` part of
   * {@link Locale}.
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::string basename() const;
  template<typename W>
  inline void basename_write(W& writeable_output) const;

  /**
   * Returns a string representation of the unicode extension.
   *
   * See the [Rust documentation for `extensions`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#structfield.extensions) for more information.
   */
  inline std::optional<std::string> get_unicode_extension(std::string_view s) const;
  template<typename W>
  inline std::optional<std::monostate> get_unicode_extension_write(std::string_view s, W& writeable_output) const;

  /**
   * Set a Unicode extension.
   *
   * See the [Rust documentation for `extensions`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#structfield.extensions) for more information.
   */
  inline std::optional<std::monostate> set_unicode_extension(std::string_view k, std::string_view v);

  /**
   * Returns a string representation of {@link Locale} language.
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::string language() const;
  template<typename W>
  inline void language_write(W& writeable_output) const;

  /**
   * Set the language part of the {@link Locale}.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> set_language(std::string_view s);

  /**
   * Returns a string representation of {@link Locale} region.
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::optional<std::string> region() const;
  template<typename W>
  inline std::optional<std::monostate> region_write(W& writeable_output) const;

  /**
   * Set the region part of the {@link Locale}.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> set_region(std::string_view s);

  /**
   * Returns a string representation of {@link Locale} script.
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::optional<std::string> script() const;
  template<typename W>
  inline std::optional<std::monostate> script_write(W& writeable_output) const;

  /**
   * Set the script part of the {@link Locale}. Pass an empty string to remove the script.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> set_script(std::string_view s);

  /**
   * Returns a string representation of the {@link Locale} variants.
   *
   * See the [Rust documentation for `Variants`](https://docs.rs/icu/2.2.0/icu/locale/struct.Variants.html) for more information.
   */
  inline std::string variants() const;
  template<typename W>
  inline void variants_write(W& writeable_output) const;

  /**
   * Returns the number of variants in this {@link Locale}.
   *
   * See the [Rust documentation for `Variants`](https://docs.rs/icu/2.2.0/icu/locale/struct.Variants.html) for more information.
   */
  inline size_t variant_count() const;

  /**
   * Returns the variant at the given index, or nothing if the index is out of bounds.
   *
   * See the [Rust documentation for `Variants`](https://docs.rs/icu/2.2.0/icu/locale/struct.Variants.html) for more information.
   */
  inline std::optional<std::string> variant_at(size_t index) const;
  template<typename W>
  inline std::optional<std::monostate> variant_at_write(size_t index, W& writeable_output) const;

  /**
   * Returns whether the {@link Locale} has a specific variant.
   *
   * See the [Rust documentation for `Variants`](https://docs.rs/icu/2.2.0/icu/locale/struct.Variants.html) for more information.
   */
  inline bool has_variant(std::string_view s) const;

  /**
   * Adds a variant to the {@link Locale}.
   *
   * Returns an error if the variant string is invalid.
   * Returns `true` if the variant was added, `false` if already present.
   *
   * See the [Rust documentation for `push`](https://docs.rs/icu/2.2.0/icu/locale/struct.Variants.html#method.push) for more information.
   */
  inline icu4x::diplomat::result<bool, icu4x::LocaleParseError> add_variant(std::string_view s);

  /**
   * Removes a variant from the {@link Locale}.
   *
   * Returns `true` if the variant was removed, `false` if not present.
   * Returns `false` for invalid variant strings (they cannot exist in the locale).
   *
   * See the [Rust documentation for `remove`](https://docs.rs/icu/2.2.0/icu/locale/struct.Variants.html#method.remove) for more information.
   */
  inline bool remove_variant(std::string_view s);

  /**
   * Clears all variants from the {@link Locale}.
   *
   * See the [Rust documentation for `clear`](https://docs.rs/icu/2.2.0/icu/locale/struct.Variants.html#method.clear) for more information.
   */
  inline void clear_variants();

  /**
   * Normalizes a locale string.
   *
   * See the [Rust documentation for `normalize`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.normalize) for more information.
   */
  inline static icu4x::diplomat::result<std::string, icu4x::LocaleParseError> normalize(std::string_view s);
  template<typename W>
  inline static icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> normalize_write(std::string_view s, W& writeable_output);

  /**
   * Returns a string representation of {@link Locale}.
   *
   * See the [Rust documentation for `write_to`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.write_to) for more information.
   */
  inline std::string to_string() const;
  template<typename W>
  inline void to_string_write(W& writeable_output) const;

  /**
   * See the [Rust documentation for `normalizing_eq`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.normalizing_eq) for more information.
   */
  inline bool normalizing_eq(std::string_view other) const;

  /**
   * See the [Rust documentation for `strict_cmp`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.strict_cmp) for more information.
   */
  inline int8_t compare_to_string(std::string_view other) const;

  /**
   * See the [Rust documentation for `total_cmp`](https://docs.rs/icu/2.2.0/icu/locale/struct.Locale.html#method.total_cmp) for more information.
   */
  inline int8_t compare_to(const icu4x::Locale& other) const;
  inline bool operator==(const icu4x::Locale& other) const;
  inline bool operator!=(const icu4x::Locale& other) const;
  inline bool operator<=(const icu4x::Locale& other) const;
  inline bool operator>=(const icu4x::Locale& other) const;
  inline bool operator<(const icu4x::Locale& other) const;
  inline bool operator>(const icu4x::Locale& other) const;

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
#endif // ICU4X_Locale_D_HPP
